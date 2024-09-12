#!/bin/bash
if [ $# -eq 0 ]; then
    echo "Error: You must provide a file to be compiled as argument"
    exit 1
fi

#move into the scripts folder if you're not there already
cd scripts &>/dev/null


if [ -z "$2" ]; then
    echo "Original with no CFI"
    ADD_SHADOW_STACK=0
    new_mode="SHADOWSTACK_XXX"
else
    echo "Add CFI"
    ADD_SHADOW_STACK=1
    case "$2" in       
        "page")
            echo "SS_PAGE"
            PROTECT_MECHANISM="SS_PAGE"
            new_mode="SHADOWSTACK_PAGE"
            ;;
        "teeplc")
            echo "SS_TEEPLC"
            PROTECT_MECHANISM="SS_TEEPLC"
            new_mode="SHADOWSTACK_TEEPLC"
            ;;
        *)
            echo "unknown type"
            ADD_SHADOW_STACK=0
            new_mode="SHADOWSTACK_XXX"
            ;;
    esac
fi
sed -i "s/#define SHADOWSTACK_[A-Za-z0-9_]*\b/#define $new_mode/" ../core/shadowstack.h

OPENPLC_PLATFORM=$(cat openplc_platform)
ETHERCAT_OPT=$(cat ethercat)
OPENPLC_DRIVER=$(cat openplc_driver)

#store the active program filename
echo "$1" > ../active_program
targetSTfile="$1"
#compiling the ST file into C
cd ..
echo "Optimizing ST program..."
./st_optimizer ./st_files/"$1" ./st_files/"$1"
echo "Generating C files..."
./iec2c -f -l -p -r -R -a ./st_files/"$1"
if [ $? -ne 0 ]; then
    echo "Error generating C files"
    echo "Compilation finished with errors!"
    exit 1
fi

# stick reference to ethercat_src in there for CoE access etc functionality that needs to be accessed from PLC
if [ "$ETHERCAT_OPT" = "ethercat" ]; then
    sed -i '7s/^/#include "ethercat_src.h" /' Res0.c
fi

echo "Moving Files..."
mv -f POUS.c POUS.h LOCATED_VARIABLES.h VARIABLES.csv Config0.c Config0.h Res0.c ./core/
if [ $? -ne 0 ]; then
    echo "Error moving files"
    echo "Compilation finished with errors!"
    exit 1
fi

if [ "$ETHERCAT_OPT" = "ethercat" ]; then
    echo "Including EtherCAT"
    ETHERCAT_INC="-L../../utils/ethercat_src/build/lib -lethercat_src -I../../utils/ethercat_src/src -D _ethercat_src"
else
    ETHERCAT_INC=""
fi

#compiling for each platform
cd core
if [ "$OPENPLC_PLATFORM" = "win" ]; then
    echo "Compiling for Windows"
    echo "Generating object files..."
    g++ -I ./lib -c Config0.c -w
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    g++ -I ./lib -c Res0.c -w
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Generating glueVars..."
    ./glue_generator
    echo "Compiling main program..."
    g++ *.cpp *.o -o openplc -I ./lib -pthread -fpermissive -I /usr/local/include/modbus -L /usr/local/lib -lmodbus -w
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Compilation finished successfully!"
    exit 0
    
elif [ "$OPENPLC_PLATFORM" = "linux" ]; then
    echo "Compiling for Linux"
    if [ "$ADD_SHADOW_STACK" = 1 ]; then
        echo "Trying add shadow stack"
        sudo rm -r /var/log/kern.log /var/log/messages /var/log/syslog
        g++ -std=gnu++11 -I ./lib -g -O0 -c shadowstack.c -fPIC -shared -o libshadowstack.so -pthread -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w -Wl,--no-as-needed -ldl
    fi
    echo "Generating object files..."
    if [ "$OPENPLC_DRIVER" = "sl_rp4" ]; then
        g++ -std=gnu++11 -I ./lib -c Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w -DSL_RP4
    else
        case "$PROTECT_MECHANISM" in
            "SS_TEEPLC")
                echo "----- No instrumentation -----"
                #change the control cycle to a limit cycles, e.g., 10000
                sed -i "s/while(run_openplc)/while(cycles)/" ./main.cpp
                echo "stat_funcs.so"
                g++ -std=c++11 -Wall -fno-rtti -fPIC -shared -Wno-literal-suffix -I/usr/lib/gcc/aarch64-linux-gnu/10/plugin/include \
                 -g -o ./instrumentation/stat_funcs.so ./instrumentation/stat_funcs.cc -ljsoncpp -I/usr/include/jsoncpp
                echo "compile func 1st time"
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_funcs.so -fplugin-arg-stat_funcs-filename="${targetSTfile/.st/_config}" Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                echo "stat_funcs.so to compile Res 1st time"
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_funcs.so -fplugin-arg-stat_funcs-filename="${targetSTfile/.st/_res}" Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                echo "Generating glueVars..."
                ./glue_generator
                echo "SS_TEEPLC main program with no instrumentation"
                g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -L./ -lshadowstack -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -w $ETHERCAT_INC -Wl,--no-as-needed -ldl
                echo "run openplc"
                ./openplc
                mv openplc "${targetSTfile/.st/_openplc_no}"
                ;;
            "SS_PAGE")
                echo "SS_PAGE Config0.c"
                g++ -std=c++11 -Wall -fno-rtti -fPIC -shared -Wno-literal-suffix -I/usr/lib/gcc/aarch64-linux-gnu/10/plugin/include -g ./instrumentation/shadowstack_page.cc -o ./instrumentation/shadowstack_page.so  
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/shadowstack_page.so Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w 
                ;;            
            *)
                g++ -std=gnu++11 -I ./lib -c Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w 
                ;;
        esac
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    if [ "$OPENPLC_DRIVER" = "sl_rp4" ]; then
        g++ -std=gnu++11 -I ./lib -c Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC -DSL_RP4
    else
        case "$PROTECT_MECHANISM" in
            "SS_TEEPLC")
                echo "----- Full instrumentation -----"
                echo "parse st file"
                python ParseST.py ../st_files/"$1" "$1" #xxx.py ../st_files/yyy.st yyy.st
                echo "priority_analysis file"
                python priority_analysis.py "${targetSTfile/.st/_config}" "$1" #xxx.py yyy_config yyy.st
                python priority_analysis.py "${targetSTfile/.st/_res}" "$1" #xxx.py yyy_config yyy.st
                echo "pWCET file"
                python pWCET.py "$1" --option_para 200
                echo "stat_prior.so"
                g++ -std=c++11 -Wall -fno-rtti -fPIC -shared -Wno-literal-suffix -I/usr/lib/gcc/aarch64-linux-gnu/10/plugin/include \
                 -g -o ./instrumentation/stat_prior.so ./instrumentation/stat_prior.cc -ljsoncpp -I/usr/include/jsoncpp
                echo "compile config 2nd time"
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_prior.so -fplugin-arg-stat_prior-filename="${targetSTfile/.st/_config}"  Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                echo "stat_prior.so to compile Res 2nd time"
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_prior.so -fplugin-arg-stat_prior-filename="${targetSTfile/.st/_res}" Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                echo "SS_TEEPLC main program"
                g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -L./ -lshadowstack -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -w $ETHERCAT_INC -Wl,--no-as-needed -ldl
                echo "run openplc"
                ./openplc
                mv openplc "${targetSTfile/.st/_openplc_full}"
                ;;
            "SS_PAGE")
                echo "SS_PAGE Res0.c"
                g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/shadowstack_page.so Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                ;;                
            *)
                g++ -std=gnu++11 -I ./lib -c Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                ;;
        esac
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Compiling main program..."
    if [ "$OPENPLC_DRIVER" = "sl_rp4" ]; then
        g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -pthread -fpermissive `pkg-config --cflags --libs libmodbus` -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC -DSL_RP4
    else
        #client.cpp dnp3.cpp enip.cpp glueVars.cpp hardware_layer.cpp iteractive_server.cpp modbus.cpp modbus_master.cpp pccc.cpp persistent_storage.cpp server.cpp
        case "$PROTECT_MECHANISM" in
            "SS_TEEPLC")
                echo "----- Variable instrumentation -----"
                echo "pWCET file"
                #python pWCET.py "$1" --option_para 200 --option_init 1
                cycle_time=1 
                return_instr=$(python pWCET.py "$1" --option_para 200 --option_init 1)
                echo "return instr is $return_instr"
                while [ "$return_instr" -eq 0 ]
                #while [ "$cycle_time" -ne 0 ]
                do
                    echo "compile config $cycle_time time"
                    g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_prior.so -fplugin-arg-stat_prior-filename="${targetSTfile/.st/_config}"  Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                    echo "stat_prior.so to compile Res $cycle_time time"
                    g++ -std=gnu++11 -I ./lib -c -fplugin=./instrumentation/stat_prior.so -fplugin-arg-stat_prior-filename="${targetSTfile/.st/_res}" Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w $ETHERCAT_INC
                    echo "SS_TEEPLC main program"
                    g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -L./ -lshadowstack -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                    -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -w $ETHERCAT_INC -Wl,--no-as-needed -ldl
                    echo "run openplc"
                    ./openplc
                    return_instr=$(python pWCET.py "$1" --option_para 200 --option_init 1)
                    ((cycle_time++))
                    echo "return instr is $return_instr"
                done
                echo "----- Finally compile -----"
                sed -i "s/while(cycles)/while(run_openplc)/" ./main.cpp
                g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -L./ -lshadowstack -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -w $ETHERCAT_INC -Wl,--no-as-needed -ldl
                mv openplc "${targetSTfile/.st/_openplc_final}"
                ;;
            "SS_PAGE")
				echo "Generating glueVars..."
				./glue_generator
                echo "SS_PAGE main program"
                g++ -std=gnu++11 -g *.cpp *.o -o openplc -I ./lib -L./ -lshadowstack -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -I/home/pi/OpenPLC_v3/webserver/core/lib -w $ETHERCAT_INC -Wl,--no-as-needed -ldl
                ;;
            *)  
				sed -i "s/while(cycles)/while(run_openplc)/" ./main.cpp
                g++ -std=gnu++11 *.cpp *.o -g -o openplc -I ./lib -pthread -fpermissive `pkg-config --cflags --libs libmodbus` \
                -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -ljsoncpp -I/usr/include/jsoncpp -w $ETHERCAT_INC
                ;;
        esac
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Compilation finished successfully!"
    exit 0
    
elif [ "$OPENPLC_PLATFORM" = "rpi" ]; then
    echo "Compiling for Raspberry Pi"
    echo "Generating object files..."
    if [ "$OPENPLC_DRIVER" = "sequent" ]; then
        g++ -std=gnu++11 -I ./lib -c Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w -DSEQUENT
    else
       g++ -std=gnu++11 -I ./lib -c Config0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    if [ "$OPENPLC_DRIVER" = "sequent" ]; then
        g++ -std=gnu++11 -I ./lib -c Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w -DSEQUENT
    else
        g++ -std=gnu++11 -I ./lib -c Res0.c -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Generating glueVars..."
    ./glue_generator
    echo "Compiling main program..."
    if [ "$OPENPLC_DRIVER" = "sequent" ]; then
        g++ -DSEQUENT -std=gnu++11 *.cpp *.o -o openplc -I ./lib -lrt -lpigpio -lpthread -fpermissive `pkg-config --cflags --libs libmodbus` -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w 
    else    
        g++ -std=gnu++11 *.cpp *.o -o openplc -I ./lib -lrt -lpigpio -lpthread -fpermissive `pkg-config --cflags --libs libmodbus` -lasiodnp3 -lasiopal -lopendnp3 -lopenpal -w
    fi
    if [ $? -ne 0 ]; then
        echo "Error compiling C files"
        echo "Compilation finished with errors!"
        exit 1
    fi
    echo "Compilation finished successfully!"
    exit 0
else
    echo "Error: Undefined platform! OpenPLC can only compile for Windows, Linux and Raspberry Pi environments"
    echo "Compilation finished with errors!"
    exit 1
fi
