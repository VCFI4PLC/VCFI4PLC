#include <iostream>

// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "tree-pass.h"
#include "internal-fn.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "stdio.h"
#include "stdlib.h"
#include "rtl.h"
#include "print-rtl.h"
#include "insn-flags.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "line-map.h"
#include "genrtl.h"
#include <string>
#include <list>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <cxxabi.h>
#include <json/json.h>
#include <fstream>
#include <list>


/* In this file, there are two passes.
 * The first one func_name_inst_pass reads all function names
 *      and save the names into myfuncList
 * The second one func_inst_pass searches all branches for all functions
 *      and count and save the called number of each functions into "filename.json"
 * Finally, it save the sum of count number to "filename_gran.txt" and all functions to "filename_funcs.txt"
 * 
 * In ParseST.py, we only save the correlations of variables in control logic programs, 
 * here we find the correlations of functions in resources
 */

int plugin_is_GPL_compatible;

const char* demangle(const char* mangled_name){
    int status;
    char* demangle_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);
    if(status==0){
        char* paren_pos =strchr(demangle_name,'(');
        if(paren_pos!=NULL){
            *paren_pos = '\0';
        }
        
        return demangle_name;
    }else{
        return mangled_name;
    }
    
}

char *input_filename;

struct myfunc{
    int count;
    int priority;
    std::list<std::string> source_funcs;
};


std::map<std::string, myfunc> myfuncList; 

bool comparePriority(const std::pair<std::string, myfunc>& func1, const std::pair<std::string, myfunc>& func2){
    return func1.second.priority > func2.second.priority;
}

namespace
{

    //find all function names
    const pass_data function_name_all = { 
        GIMPLE_PASS, /* type */
        "func_name_inst_pass", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_NONE, /* tv_id */
        PROP_gimple_any,
        0, /* properties_provided */
        0, /* properties_destroyed */
        0,  //todo_flags_start 
        0, /* todo_flags_finish */
    };
    
    struct func_name_inst_pass : gimple_opt_pass{

        func_name_inst_pass(gcc::context *ctx)
            : gimple_opt_pass(function_name_all, ctx)
        {    
        }
        

        virtual unsigned int execute (function *);
        

    };    
    
    
    //count all functions
    const pass_data function_count_stat = { 
        RTL_PASS, /* type */
        "func_inst_pass", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_NONE, /* tv_id */
        (PROP_ssa | PROP_gimple_any | PROP_cfg),
        0, /* properties_provided */
        0, /* properties_destroyed */
        0,  //todo_flags_start 
        0, /* todo_flags_finish */
    };
    
    struct func_inst_pass : rtl_opt_pass{

        func_inst_pass(gcc::context *ctx)
            : rtl_opt_pass(function_count_stat, ctx)
        {    
        } 

        virtual unsigned int execute (function *);

    };

}



unsigned int func_name_inst_pass::execute (function *fun)
{
    //printf("func_name_inst_pass.\n");
    const char *function_name = IDENTIFIER_POINTER(DECL_NAME(fun->decl));
    
        
    if (function_name && TREE_CODE(fun->decl) == FUNCTION_DECL){
            //printf("current function name is %s\n",function_name);
            std::string added_func = function_name;
            myfuncList[added_func] = {1, 0};

    }
    /*
    //write functions to files
    char filename[1000];
    snprintf(filename, sizeof(filename), "/home/pi/OpenPLC_v3/webserver/core/dynamic_link/%s.S", function_name);
    
    FILE *file = fopen(filename, "w");
    if(!file){
		printf("Could not open file %s for writing.\n", filename);
		return 0;
	}
	
	fprintf(file, "// Assemble code for function: %s\n", function_name);
	
	basic_block bb;
	FOR_EACH_BB_FN(bb, fun){
		rtx_insn *insn;
		FOR_BB_INSNS(bb, insn){
			if (INSN_P(insn)){
				pretty_printer pp;
				pp_initialize(&pp,file);
				print_rtl_insn(&pp, insn);
				pp_flush(&pp);
			}
		}	
	}
	
	
	rtx_insn *insn;
    for(insn = get_insns(); insn!=get_last_insn(); insn = NEXT_INSN(insn)){
		if(INSN_P(insn)){ print_rtl(file, insn); }
	}
    */
    return 0;
}


//std::sort(myfuncList.begin(), myfuncList.end(), comparePriority);

unsigned int func_inst_pass::execute (function *fun)
{
    //printf("func_inst_pass.\n");
    rtx_insn *insn;
    //int num = 0;
    const char *function_name = IDENTIFIER_POINTER(DECL_NAME(fun->decl));
    //printf("current function name is %s\n",function_name);
    for(insn = get_insns(); insn!=get_last_insn(); insn = NEXT_INSN(insn)){
        if(GET_CODE(insn) == CALL_INSN){
            //num++;
            //rtx current_insn = XEXP(insn,0);
            //CALL_INSN = 11; JUMP_INSN = 10; INSN = 9; NOTE = 15; GET_CODE(XEXP(insn,0)) = 9
            
            
            //enum rtx_code code = GET_CODE(current_insn);
            //const char *name = GET_RTX_NAME(code);
            
            //tree symbol = SYMBOL_REF_DECL(current_insn);
            
            rtx callExpr = PATTERN(insn); 
            //rtx ccur_insn = XEXP(callExpr,0);
            /*
            enum rtx_code code = GET_CODE(callExpr);
            enum rtx_code code2 = GET_CODE(ccur_insn);
            const char *name = GET_RTX_NAME(code); //parallel=17
            const char *target_name = GET_RTX_NAME(code2); //expr_list=3
            */
            if (GET_CODE(callExpr)==PARALLEL){
                //printf("code is PARALLEL.\n");
                for(int i = 0; i < XVECLEN(callExpr,0); i++){
                    rtx subExpr = XVECEXP(callExpr, 0, i);
                    rtx sub_insn = XEXP(subExpr,0);
                    /*
                    enum rtx_code subcode = GET_CODE(subExpr);
                    enum rtx_code subcode2 = GET_CODE(sub_insn);
                    const char *subname = GET_RTX_NAME(subcode); //subname = set | unspec | clobber
                    const char *subtarget_name = GET_RTX_NAME(subcode2); //subtarget_name = mem | value | reg
                    printf("get in XVECLEN is %s and %d and %d and %s\n", subtarget_name, GET_CODE(subExpr), GET_CODE(sub_insn), subname);
                    */
                    /*
                    if(GET_CODE(subExpr)==UNSPEC && GET_CODE(sub_insn)==VALUE){
                        HOST_WIDE_INT func_addr = INTVAL(sub_insn);
                        printf("subtarget_addr is %ld\n", func_addr);
                    }*/
                    if(GET_CODE(subExpr)==CALL){// && GET_CODE(sub_insn)==MEM
                        rtx mempointer = XEXP(sub_insn,0);
                        //enum rtx_code submem = GET_CODE(mempointer);
                        //const char *submem_name = GET_RTX_NAME(submem); //symbol_ref
                        const char *submem_target = XSTR(mempointer,0);
                        //printf("submem_name is %s\n", submem_name);
                        //printf("submem_target is %s\n", submem_target);
                        
                        std::string itemNameToFind = demangle(submem_target);
                        auto foundFunc = myfuncList.find(itemNameToFind);
                        if(foundFunc != myfuncList.end()){
                            //std::cout<< "    Find "<<itemNameToFind<< " in "<<submem_target<<std::endl;
                            foundFunc->second.count++;
                            foundFunc->second.source_funcs.push_front(function_name);
                        }else{
                            /*
                            for(const auto &func : myfuncList){
                                const myfunc& _myfunc = func.second;
                                std::cout<< "Function: "<<func.first<< ", Count: "<<_myfunc.count<< ", Priority: "<< _myfunc.priority<<std::endl;
                            }*/
                            //std::cout<< "Cannot find "<<itemNameToFind<< " in "<<submem_target<<std::endl;
                        }
                        
                        break;
                    }
                    if(GET_CODE(subExpr)==SET){// && GET_CODE(sub_insn)==REG
                        rtx regpointer = XEXP(XEXP(SET_SRC(subExpr),0),0);//SET->CALL->MEM->SYMBOL_REF
                        //enum rtx_code subreg = GET_CODE(regpointer);
                        //const char *subreg_name = GET_RTX_NAME(subreg); 
                        const char *subreg_target = XSTR(regpointer,0);
                        //printf("subreg_name is %s\n", subreg_name);
                        //printf("subreg_target is %s\n", subreg_target);
                        std::string itemNameToFind = demangle(subreg_target);
                        auto foundFunc = myfuncList.find(itemNameToFind);
                        if(foundFunc != myfuncList.end()){
                            //std::cout<< "    Find "<<itemNameToFind<< " in "<<subreg_target<<std::endl;
                            foundFunc->second.count++;
                            foundFunc->second.source_funcs.push_front(function_name);
                        }else{
                            //std::cout<< "Cannot find "<<itemNameToFind<< " in "<<subreg_target<<std::endl;
                        }
                        break;
                    }
                    
                }
            }
        
        }
        
    }

    static unsigned int hasExecuted = myfuncList.size();
    //printf("hasExecuted is %d\n", hasExecuted);
    if(hasExecuted>1){
        hasExecuted -= 1;
    }else{
        std::vector<std::pair<std::string, myfunc>> vecMyFuncList(myfuncList.begin(), myfuncList.end());
        
        std::sort(vecMyFuncList.begin(), vecMyFuncList.end(), comparePriority);
        
        //std::cout<<"myfuncList lenth is "<<myfuncList.size()<<std::endl;
        
        Json::Value root;
        Json::StreamWriterBuilder writerBuilder;
        Json::Value item;
        int ss_num = 0;
        for(const auto &func : myfuncList){
            Json::Value arr(Json::arrayValue);
            const myfunc& _myfunc = func.second;
            ss_num += _myfunc.count;
            //std::cout<< "Function: "<<func.first<< ", Count: "<<_myfunc.count<< ", Priority: "<< _myfunc.priority<<std::endl;
            for(std::string strin : _myfunc.source_funcs){
                //std::cout<<strin<<", ";
                arr.append(strin);
            }
            //std::cout<<std::endl;
            item["name"] = func.first;
            item["count"] = _myfunc.count;
            item["source_funcs"] = arr;
            root.append(item);
        }
        std::string objectiveJsonFile = std::string("../json/") + input_filename + std::string(".json");
        std::ofstream outputFile(objectiveJsonFile, std::ios::out | std::ios::trunc); //like 'w+' in python
        if(outputFile.is_open()){
            std::string json_str = Json::writeString(writerBuilder,root);
            outputFile << json_str;
            outputFile.close();
            //std::cout<<"JSON write finish"<<std::endl;
        }else{
            //std::cerr<<"Unable to open file"<<std::endl;
            return 1;
        }
        std::cout<<"ss_num is "<<ss_num<<std::endl;
        std::string objectiveGranTxt = std::string("../json/") + input_filename + std::string("_gran.txt");
        std::ofstream ssoutputFile(objectiveGranTxt, std::ios::out | std::ios::trunc); 
        if(!ssoutputFile){
            std::cerr << "Error opening gran.txt"<<std::endl;
        }
        ssoutputFile << ss_num<<'\n';
        ssoutputFile.close();
        
        std::string objectiveFuncTxt = std::string("../json/") + input_filename + std::string("_funcs.txt");
        std::ofstream funcoutputFile(objectiveFuncTxt, std::ios::out | std::ios::trunc); 
        if(!funcoutputFile){
            std::cerr << "Error opening gran.txt"<<std::endl;
        }
        for(const auto &func : vecMyFuncList){
            funcoutputFile << func.first<<' ';
        }
        funcoutputFile << '\n';
        funcoutputFile.close();
    }
    return 0;
}



int plugin_init (struct plugin_name_args *plugin_info,
        struct plugin_gcc_version *version)
{
    if (!plugin_default_version_check (version, &gcc_version))
    {
        std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
        return 1;
    }
    
    
    
    struct plugin_argument *argv = plugin_info->argv;
    if(!strcmp(argv[0].key,"filename")) {
        input_filename = argv[0].value;
    }
    //std::cout<<"Input file name is "<<input_filename<<std::endl;
    
    
    //cfg -> ssa 
    struct register_pass_info func_name_info;
    func_name_info.pass = new func_name_inst_pass(g);
    func_name_info.reference_pass_name = "ssa";
    func_name_info.ref_pass_instance_number = 1;
    func_name_info.pos_op = PASS_POS_INSERT_AFTER;
    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &func_name_info);

    struct register_pass_info func_pass_info;
    func_pass_info.pass = new func_inst_pass(g);
    func_pass_info.reference_pass_name = "sched2";
    func_pass_info.ref_pass_instance_number = 1;
    func_pass_info.pos_op = PASS_POS_INSERT_AFTER;
    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &func_pass_info);


    return 0;
}
