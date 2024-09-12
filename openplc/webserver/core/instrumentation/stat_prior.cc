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
#include <json/value.h>
#include <sstream>
/* In this file, there are two passes.
 * The first one func_prior_inst_pass reads "new_filename.json" to get the priority of each func
 *      and read "filename_gran.txt" to get the granularity
 *      then it sorts the funcs and calculates the chosen funcs based on the granularity
 * The second one epol_inst_pass instrument the code with the special granularity
 */


int plugin_is_GPL_compatible;

int ss_num=0;

struct myfunc{
    int count;
    int priority;
};
std::map<std::string, myfunc> myfuncList; 
bool comparePriority(const std::pair<std::string, myfunc>& func1, const std::pair<std::string, myfunc>& func2){
    return func1.second.priority > func2.second.priority;
}
std::vector<std::pair<std::string, myfunc>> vecMyFuncList;

std::list<std::string> ssfuncList;

char *input_filename;

std::vector<std::string> functionNames;
namespace
{
    
    //find all function names
    const pass_data function_prior_all = { 
        GIMPLE_PASS, /* type */
        "func_prior_inst_pass", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_NONE, /* tv_id */
        PROP_gimple_any,
        0, /* properties_provided */
        0, /* properties_destroyed */
        0,  //todo_flags_start 
        0, /* todo_flags_finish */
    };
    
    struct func_prior_inst_pass : gimple_opt_pass{

        func_prior_inst_pass(gcc::context *ctx)
            : gimple_opt_pass(function_prior_all, ctx)
        {    
        }
        

        virtual unsigned int execute (function *);
        

    };    
    
    

    //shadow stack instrumentation
    const pass_data pass_epologue_instrument = { 
        RTL_PASS, /* type */
        "epol_inst_pass", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_NONE, /* tv_id */
        // (PROP_ssa | PROP_gimple_leh | PROP_cfg | PROP_gimple_lcx | PROP_gimple_lvec | PROP_gimple_lva), /* properties_required */
        (PROP_ssa | PROP_gimple_any | PROP_cfg),
        0, /* properties_provided */
        0, /* properties_destroyed */
        0,  //todo_flags_start 
        0, /* todo_flags_finish */
    };

    /* Description of RTL pass.  */

    struct epol_inst_pass : rtl_opt_pass{

        epol_inst_pass(gcc::context *ctx)
            : rtl_opt_pass(pass_epologue_instrument, ctx)
        {    
        } 

        virtual unsigned int execute (function *);

    };

}



unsigned int func_prior_inst_pass::execute (function *fun)
{
    //printf("func_name_inst_pass.\n");
    
    static bool hasExecuted = false;
    //printf("hasExecuted is %d\n", hasExecuted);
    if(!hasExecuted){
        //for(const auto& func : myfuncList){
            //std::cout<< "Ori Function: "<<func.first<< ", Count: "<<func.second.count<< ", Priority: "<< func.second.priority<<std::endl;
        //}  
        
        vecMyFuncList.assign(myfuncList.begin(), myfuncList.end());
        
        std::sort(vecMyFuncList.begin(), vecMyFuncList.end(), comparePriority);
        
        int current_count = 0;
        for(const auto& func : vecMyFuncList){
            current_count += func.second.count;
            if (current_count < ss_num ){
                ssfuncList.push_front(func.first);
            }else{
                break;
            }
            //std::cout<< "Ori Function: "<<func.first<< ", Count: "<<func.second.count<< ", Priority: "<< func.second.priority<<std::endl;
        }
        //for(const auto& func : ssfuncList){        
            //std::cout<< "Function: "<<func<<std::endl;
        //}
        /*
        const char *function_name = IDENTIFIER_POINTER(DECL_NAME(fun->decl));
        
            
        if (function_name && TREE_CODE(fun->decl) == FUNCTION_DECL){
                //printf("current function name is %s\n",function_name);
                std::string added_func = function_name;
                myfuncList[added_func] = {1, 0};

        }
        */

        hasExecuted = true;
    }
       
    return 0;
}





unsigned int epol_inst_pass::execute (function *fun)
{
    std::string function_name = IDENTIFIER_POINTER(DECL_NAME(fun->decl));
    std::string ss_save = "shadow_stack_save";
    std::string ss_restore = "shadow_stack_restore";
    std::string main = "main";
    std::string substring = "handleConnections";
	
	int tee_flag = 0; 
    
    rtx_insn *insn;
    //std::cout << function_name << std::endl;
    
    std::list<rtx_insn*> to_be_remove;
    
    //filter shadow stack operation trampoline function
    if(function_name == ss_save ||\
        function_name == ss_restore ||\
        function_name == main ||\
        function_name.find(substring) != std::string::npos){
        return 0;
    }
    //printf("epol_inst_pass. \n");
    
    
    
    if(std::find(functionNames.begin(), functionNames.end(), function_name) != functionNames.end()){
        //std::cout<<"Function: "<<function_name<<" in the list"<<std::endl;
        tee_flag = 1;
    }else{
		tee_flag = 0;
        //std::cout<<"Function: "<<function_name<<" not in the list"<<std::endl;
        //return 0;
    }
    
    
    for(insn = get_insns(); insn!=get_last_insn(); insn = NEXT_INSN(insn)){


        //this used to find out prologue and epologue
        if(GET_CODE(insn) == NOTE){
          
            //rtunion note_data = NOTE_DATA(insn);
            enum insn_note note = (enum insn_note)NOTE_KIND(insn);
            
            //instrument prologue to insert shadow stack saving 
            if(note == NOTE_INSN_PROLOGUE_END){

                // following codes can insert function call into rtl

                //emit context x0 saving and restoring codes
                start_sequence();
                rtx seq;

                //push rx
                rtx push_x0, push_x29, set_x29;
                rtx_insn *push_insn, *push_insn_x29, *set_insn_x29;


                rtx minus_sp;
                rtx_insn *minus_insn;
                //sub sp, sp, #32 (64 bits aligned)
                minus_sp = gen_rtx_SET(stack_pointer_rtx, \
                    gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(-32)));
                minus_insn = emit_insn_after(minus_sp, insn);

                //str lr, [sp]
                rtx push_lr_rtx;
                rtx_insn *push_lr_insn;
                push_lr_rtx = gen_rtx_SET(gen_rtx_MEM(DImode, stack_pointer_rtx), gen_rtx_REG(DImode, 30));
                push_lr_insn = emit_insn_after(push_lr_rtx, minus_insn);
                
                //str x29, [sp, #8]
                push_x29 = gen_rtx_SET(gen_rtx_MEM(DImode, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(8))), \
                    gen_rtx_REG(DImode, 29));
                push_insn_x29 = emit_insn_after(push_x29, push_lr_insn);
                
                //mov x29, sp
                set_x29 = gen_rtx_SET(gen_rtx_REG(DImode, 29), stack_pointer_rtx);
                set_insn_x29 = emit_insn_after(set_x29, push_insn_x29);

                //str x0, [sp, #16]
                push_x0 = gen_rtx_SET(gen_rtx_MEM(DImode, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))), \
                    gen_rtx_REG(DImode, 0));
                push_insn = emit_insn_after(push_x0, set_insn_x29);
				
				
				rtx ldr_lr_rtx;
				rtx_insn* ldr_lr_insn;
				/*if(tee_flag == 0){
					//use register x26
					//str x0, [x26]
					rtx store_x0;
					rtx_insn *store_insn_x0;
					store_x0 = gen_rtx_SET(gen_rtx_MEM(DImode, gen_rtx_REG(DImode, 26)), \
						gen_rtx_REG(DImode, 0));
					store_insn_x0 = emit_insn_after(store_x0, push_insn);

					
					//add x26, x26, #8
					rtx add_x26;
					rtx_insn *add_insn_x26;
					add_x26 = gen_rtx_SET(gen_rtx_REG(DImode, 26), gen_rtx_PLUS(DImode, gen_rtx_REG(DImode, 26), GEN_INT(8)));
					add_insn_x26 = emit_insn_after(add_x26, store_insn_x0);
					
				
					//replace post_inc with two separate instruction

					//ldr lr, [sp]
					ldr_lr_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 30), gen_rtx_MEM(DImode, stack_pointer_rtx));
					ldr_lr_insn = emit_insn_after(ldr_lr_rtx, add_insn_x26);
				}else{
				
					//ldr lr, [sp]
					ldr_lr_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 30), gen_rtx_MEM(DImode, stack_pointer_rtx));
					ldr_lr_insn = emit_insn_after(ldr_lr_rtx, push_insn);
				}*/
				//ldr lr, [sp]
				ldr_lr_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 30), gen_rtx_MEM(DImode, stack_pointer_rtx));
				ldr_lr_insn = emit_insn_after(ldr_lr_rtx, push_insn); 
                rtx pop_x0, pop_x29;
                rtx_insn *pop_insn, *pop_insn_x29; 
                
                //ldr x29, [sp+8]
                pop_x29 = gen_rtx_SET(gen_rtx_REG(DImode, 29), gen_rtx_MEM(DImode, \
                    gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(8))));
                pop_insn_x29 = emit_insn_after(pop_x29, ldr_lr_insn);
                               
                //ldr x0, [sp+16]
                pop_x0 = gen_rtx_SET(gen_rtx_REG(DImode, 0), gen_rtx_MEM(DImode, \
                    gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))));
                pop_insn = emit_insn_after(pop_x0, pop_insn_x29);

                //add sp, sp, #32
                rtx plus_sp;
                //rtx_insn *plus_insn;
                plus_sp = gen_rtx_SET(stack_pointer_rtx, \
                    gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(32)));                
                //plus_insn = emit_insn_after(plus_sp, pop_insn);
                emit_insn_after(plus_sp, pop_insn);
                
                seq = get_insns();
                end_sequence();
                emit_insn_after(seq, insn);

                //emit shadow stack saving function call
                //prologue_num -= 1;
                

                if(tee_flag == 1){
					start_sequence();
					rtx internal_seq;
					emit_library_call(gen_rtx_SYMBOL_REF(Pmode, "shadow_stack_save"), LCT_NORMAL,
								  VOIDmode, gen_rtx_REG(Pmode, 30), Pmode);  
					
					internal_seq = get_insns();
					end_sequence();
					emit_insn_after(internal_seq, push_insn);
				}else{
					start_sequence();
					rtx internal_seq;
					emit_library_call(gen_rtx_SYMBOL_REF(Pmode, "page_shadow_stack_save"), LCT_NORMAL,
								  VOIDmode, gen_rtx_REG(Pmode, 30), Pmode);  
					
					internal_seq = get_insns();
					end_sequence();
					emit_insn_after(internal_seq, push_insn);
				}
                //end command
            }

        }


        //instrument epilogue to insert shadown stack restoring
        if(GET_CODE(insn) == JUMP_INSN){


            rtx internal_exp = XEXP(insn,3);

            switch(GET_CODE(internal_exp)){
                case SIMPLE_RETURN:
                case RETURN:{
                    
                    
                    start_sequence();
                    
					rtx push_x0, push_x29, set_x29;
					rtx_insn *push_insn, *push_insn_x29, *set_insn_x29;
					
                    rtx minus_sp;
                    rtx_insn *minus_insn;
                    //sub sp, sp, #32 (64 bits aligned)
                    minus_sp = gen_rtx_SET(stack_pointer_rtx, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(-32)));
                    minus_insn = emit_insn(minus_sp);
                    
                    //str lr, [sp]
					rtx push_lr_rtx;
					rtx_insn *push_lr_insn;
					push_lr_rtx = gen_rtx_SET(gen_rtx_MEM(DImode, stack_pointer_rtx), gen_rtx_REG(DImode, 30));
					push_lr_insn = emit_insn_after(push_lr_rtx, minus_insn);
					
					//str x29, [sp, #8]
					push_x29 = gen_rtx_SET(gen_rtx_MEM(DImode, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(8))), \
						gen_rtx_REG(DImode, 29));
					push_insn_x29 = emit_insn_after(push_x29, push_lr_insn);
					
					//mov x29, sp
					set_x29 = gen_rtx_SET(gen_rtx_REG(DImode, 29), stack_pointer_rtx);
					set_insn_x29 = emit_insn_after(set_x29, push_insn_x29);

					//str x0, [sp, #16]
					push_x0 = gen_rtx_SET(gen_rtx_MEM(DImode, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))), \
						gen_rtx_REG(DImode, 0));
					push_insn = emit_insn_after(push_x0, set_insn_x29);


                    
                    
                    rtx mov_rtl, ldr_x0_rtx;
					rtx_insn *mov_insn, *ldr_x0_insn;
					/*
					if(tee_flag == 0){
						//sub x26, x26, #8
						rtx minus_x26;
						rtx_insn *minus_insn_x26;
						minus_x26 = gen_rtx_SET(gen_rtx_REG(DImode, 26), gen_rtx_PLUS(DImode, gen_rtx_REG(DImode, 26), GEN_INT(-8)));
						minus_insn_x26 = emit_insn_after(minus_x26, push_insn);
						
						//ldr x0, [x26]
						rtx ldr_x0;
						rtx_insn *ldr_insn_x0;
						ldr_x0 = gen_rtx_SET(gen_rtx_REG(DImode, 0), gen_rtx_MEM(DImode, \
									gen_rtx_REG(DImode, 26)));
						ldr_insn_x0 = emit_insn_after(ldr_x0, minus_insn_x26);
						


						// mov lr, x0
						mov_rtl = gen_rtx_SET(gen_rtx_REG(DImode,30), gen_rtx_REG(DImode,0));
						mov_insn = emit_insn_after(mov_rtl, ldr_insn_x0);
					}else{
						// mov lr, x0
						mov_rtl = gen_rtx_SET(gen_rtx_REG(DImode,30), gen_rtx_REG(DImode,0));
						mov_insn = emit_insn_after(mov_rtl, push_insn);
					}
					*/
					// mov lr, x0
					mov_rtl = gen_rtx_SET(gen_rtx_REG(DImode,30), gen_rtx_REG(DImode,0));
					mov_insn = emit_insn_after(mov_rtl, push_insn);
				
					// ldr x0, [sp+16]
					ldr_x0_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 0), gen_rtx_MEM(DImode, \
					gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))));
					ldr_x0_insn = emit_insn_after(ldr_x0_rtx, mov_insn);	
					
					/*
					rtx ldr_x0_rtx, ldr_lr_rtx;
                    rtx_insn *ldr_x0_insn, *ldr_lr_insn;
					if(tee_flag == 1){
						// mov lr, x0
						mov_rtl = gen_rtx_SET(gen_rtx_REG(DImode,30), gen_rtx_REG(DImode,0));
						mov_insn = emit_insn_after(mov_rtl, push_insn);
					
						// ldr x0, [sp+16]
						ldr_x0_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 0), gen_rtx_MEM(DImode, \
						gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))));
						ldr_x0_insn = emit_insn_after(ldr_x0_rtx, mov_insn);						
					}else{
						//ldr lr, [sp]
						ldr_lr_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 30), gen_rtx_MEM(DImode, stack_pointer_rtx));
						ldr_lr_insn = emit_insn_after(ldr_lr_rtx, push_insn); 
						
						// ldr x0, [sp+16]
						ldr_x0_rtx = gen_rtx_SET(gen_rtx_REG(DImode, 0), gen_rtx_MEM(DImode, \
						gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(16))));
						ldr_x0_insn = emit_insn_after(ldr_x0_rtx, ldr_lr_insn);
					}
					*/
					
					rtx pop_x0, pop_x29;
					rtx_insn *pop_insn, *pop_insn_x29; 
					
					//ldr x29, [sp+8]
					pop_x29 = gen_rtx_SET(gen_rtx_REG(DImode, 29), gen_rtx_MEM(DImode, \
						gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(8))));
					pop_insn_x29 = emit_insn_after(pop_x29, ldr_x0_insn);
								   

                    //add sp, sp, #32
                    rtx plus_sp;
                    //rtx_insn *plus_insn;
                    plus_sp = gen_rtx_SET(stack_pointer_rtx, gen_rtx_PLUS(DImode, stack_pointer_rtx, GEN_INT(32)));                
                    //plus_insn = emit_insn_after(plus_sp, ldr_x0_insn);
                    emit_insn_after(plus_sp, pop_insn_x29);


                    rtx push_x0_seq;
                    push_x0_seq = get_insns();
                    end_sequence();
                    emit_insn_before(push_x0_seq, insn);

                    
                    if(tee_flag == 1){
						start_sequence();
						rtx internal_seq;
						//emit_library_call_value(gen_rtx_SYMBOL_REF(Pmode, "shadow_stack_restore"),gen_rtx_REG(DImode, 0), LCT_CONST, VOIDmode); 
						emit_library_call_value(gen_rtx_SYMBOL_REF(Pmode, "shadow_stack_restore"),gen_rtx_REG(DImode, 0), LCT_CONST, VOIDmode, \
                    gen_rtx_REG(Pmode, 30), Pmode); 
						internal_seq = get_insns();
						end_sequence();
						emit_insn_before(internal_seq, mov_insn);
					}
					else{
						
						start_sequence();
						rtx internal_seq;
						//emit_library_call_value(gen_rtx_SYMBOL_REF(Pmode, "shadow_stack_restore"),gen_rtx_REG(DImode, 0), LCT_CONST, VOIDmode); 
						emit_library_call_value(gen_rtx_SYMBOL_REF(Pmode, "page_shadow_stack_restore"),gen_rtx_REG(DImode, 0), LCT_CONST, VOIDmode, \
                    gen_rtx_REG(Pmode, 30), Pmode); 
						internal_seq = get_insns();
						end_sequence();
						emit_insn_before(internal_seq, mov_insn);
						
						/*
						start_sequence();
						rtx internal_seq;
						emit_library_call(gen_rtx_SYMBOL_REF(Pmode, "page_shadow_stack_restore"), LCT_NORMAL,
								  VOIDmode, gen_rtx_REG(Pmode, 30), Pmode);  

						internal_seq = get_insns();
						end_sequence();
						emit_insn_before(internal_seq, ldr_lr_insn);
						 
						*/
					}

                    break;
                }
                default:
                    break;
            }
        }

        continue;


    }
    
    //remove original write instruction whose address is to be masked
    std::list<rtx_insn*>::iterator iter;
    for(iter = to_be_remove.begin(); iter != to_be_remove.end(); iter++){
        remove_insn(*iter);
    }
    
    return 0;
}


std::string getLastLine(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return 0;
    }
    file.seekg(-2, std::ios_base::end);
    bool keepLooping = true;
    while (keepLooping) {
        char ch;
        file.get(ch);
        
        if((int)file.tellg()<=1){
            file.seekg(0);
            keepLooping = false;
        }
        else if (ch == '\n') {
            keepLooping = false;
        }
        else{
            file.seekg(-2, std::ios_base::cur);
        }
    }
    std::string lastLine;
    std::getline(file, lastLine);
    return lastLine;
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
    std::string newJsonFile = std::string("../json/") + std::string(input_filename) + std::string("_new.json");
    std::ifstream file(newJsonFile);
    static Json::Value jsonData;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    if(!file.is_open()){
        std::cerr << "Failed open new json" << std::endl;
        return 1;
    }
    
    try{
        bool parsingSuccessful = Json::parseFromStream(builder, file, &jsonData, &errors);
        if(!parsingSuccessful){
            std::cerr<<"Error parsing JSON: "<<errors<<std::endl;
            return 1;
        }
    } catch (const std::exception& e){
        std::cerr << "Error parsing JSON: "<<e.what() << std::endl;
        return 1;
    }
    file.close();
    if(!jsonData.isArray()){
        std::cerr<<"jsonData is not an array\n";
    }
    
    for (const auto& item: jsonData){
        const std::string functionName = item["name"].asString();
        const int count = item["count"].asInt();
        const int priority = item["priority"].asInt();
        //std::cout<<"Function name is "<<functionName<<", Count is "<<count<<", Priority is "<<priority<<std::endl;
        myfuncList[functionName] = {count, priority};
    }
    
    
    std::string granFile = std::string("../json/") + input_filename + std::string("_gran.txt");
    std::string lastLine = getLastLine(granFile);
    if (!lastLine.empty()) {
        ss_num = std::stoi(lastLine);
    }
    std::cout<<"ss_num is "<<ss_num<<std::endl;



    
    std::ifstream ssfile(granFile);
    
    if (ssfile.is_open()){
        std::string currentLine;
        while(std::getline(file, currentLine)){
            std::istringstream iss(currentLine);
            if(!(iss>>ss_num)){
                std::cerr<<"Error last line"<<std::endl;
            }
        }
        ssfile.close();
    }else{
        std::cerr<<"Error reading the number"<<std::endl;
    }
    
    std::string funcFile = std::string("../json/") + input_filename + std::string("_funcs.txt");
    std::string funcLastLine;
    funcLastLine = getLastLine(funcFile);
    std::istringstream iss(funcLastLine);
    std::string functionName;
    while (iss>>functionName){
        functionNames.push_back(functionName);
    }
    
    //cfg -> ssa 
    struct register_pass_info func_prior_info;
    func_prior_info.pass = new func_prior_inst_pass(g);
    func_prior_info.reference_pass_name = "cfg";
    func_prior_info.ref_pass_instance_number = 1;
    func_prior_info.pos_op = PASS_POS_INSERT_AFTER;
    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &func_prior_info);


    //pro_and_epilogue | *free_cfg | dbr | sched_fusion | jump

    struct register_pass_info epo_pass_info;
    epo_pass_info.pass = new epol_inst_pass(g);
    // epo_pass_info.reference_pass_name = "pro_and_epilogue"; // hook this pass with ssa builder pass
    epo_pass_info.reference_pass_name = "sched2"; // hook this pass with ssa builder pass
    // epo_pass_info.reference_pass_name = "final"; // hook this pass with ssa builder pass
    epo_pass_info.ref_pass_instance_number = 1; // Insert the pass at the specified        
    epo_pass_info.pos_op = PASS_POS_INSERT_AFTER; // After SSA is built

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &epo_pass_info);

    
    


    return 0;
}
