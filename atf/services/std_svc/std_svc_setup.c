/*
 * Copyright (c) 2014-2023, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/el3_runtime/cpu_data.h>
#include <lib/pmf/pmf.h>
#include <lib/psci/psci.h>
#include <lib/runtime_instr.h>
#include <services/drtm_svc.h>
#include <services/errata_abi_svc.h>
#include <services/pci_svc.h>
#include <services/rmmd_svc.h>
#include <services/sdei.h>
#include <services/spm_mm_svc.h>
#include <services/spmc_svc.h>
#include <services/spmd_svc.h>
#include <services/std_svc.h>
#include <services/trng_svc.h>
#include <smccc_helpers.h>
#include <tools_share/uuid.h>

/* Standard Service UUID */
static uuid_t arm_svc_uid = {
	{0x5b, 0x90, 0x8d, 0x10},
	{0x63, 0xf8},
	{0xe8, 0x47},
	0xae, 0x2d,
	{0xc0, 0xfb, 0x56, 0x41, 0xf6, 0xe2}
};

/* Setup Standard Services */
static int32_t std_svc_setup(void)
{
	uintptr_t svc_arg;
	int ret = 0;

	svc_arg = get_arm_std_svc_args(PSCI_FID_MASK);
	assert(svc_arg);

	/*
	 * PSCI is one of the specifications implemented as a Standard Service.
	 * The `psci_setup()` also does EL3 architectural setup.
	 */
	if (psci_setup((const psci_lib_args_t *)svc_arg) != PSCI_E_SUCCESS) {
		ret = 1;
	}

#if SPM_MM
	if (spm_mm_setup() != 0) {
		ret = 1;
	}
#endif

#if defined(SPD_spmd)
	if (spmd_setup() != 0) {
		ret = 1;
	}
#endif

#if ENABLE_RME
	if (rmmd_setup() != 0) {
		ret = 1;
	}
#endif

#if SDEI_SUPPORT
	/* SDEI initialisation */
	sdei_init();
#endif

#if TRNG_SUPPORT
	/* TRNG initialisation */
	trng_setup();
#endif /* TRNG_SUPPORT */

#if DRTM_SUPPORT
	if (drtm_setup() != 0) {
		ret = 1;
	}
#endif /* DRTM_SUPPORT */

	return ret;
}

/*
 * Top-level Standard Service SMC handler. This handler will in turn dispatch
 * calls to PSCI SMC handler
 */
static uintptr_t std_svc_smc_handler(uint32_t smc_fid,
			     u_register_t x1,
			     u_register_t x2,
			     u_register_t x3,
			     u_register_t x4,
			     void *cookie,
			     void *handle,
			     u_register_t flags)
{
	if (((smc_fid >> FUNCID_CC_SHIFT) & FUNCID_CC_MASK) == SMC_32) {
		/* 32-bit SMC function, clear top parameter bits */

		x1 &= UINT32_MAX;
		x2 &= UINT32_MAX;
		x3 &= UINT32_MAX;
		x4 &= UINT32_MAX;
	}

	/*
	 * Dispatch PSCI calls to PSCI SMC handler and return its return
	 * value
	 */
	if (is_psci_fid(smc_fid)) {
		uint64_t ret;

#if ENABLE_RUNTIME_INSTRUMENTATION

		/*
		 * Flush cache line so that even if CPU power down happens
		 * the timestamp update is reflected in memory.
		 */
		PMF_WRITE_TIMESTAMP(rt_instr_svc,
		    RT_INSTR_ENTER_PSCI,
		    PMF_CACHE_MAINT,
		    get_cpu_data(cpu_data_pmf_ts[CPU_DATA_PMF_TS0_IDX]));
#endif

		ret = psci_smc_handler(smc_fid, x1, x2, x3, x4,
		    cookie, handle, flags);

#if ENABLE_RUNTIME_INSTRUMENTATION
		PMF_CAPTURE_TIMESTAMP(rt_instr_svc,
		    RT_INSTR_EXIT_PSCI,
		    PMF_NO_CACHE_MAINT);
#endif

		SMC_RET1(handle, ret);
	}

#if SPM_MM
	/*
	 * Dispatch SPM calls to SPM SMC handler and return its return
	 * value
	 */
	if (is_spm_mm_fid(smc_fid)) {
		return spm_mm_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
					  handle, flags);
	}
#endif

#if defined(SPD_spmd)
	/*
	 * Dispatch FFA calls to the FFA SMC handler implemented by the SPM
	 * dispatcher and return its return value
	 */
	if (is_ffa_fid(smc_fid)) {
		return spmd_ffa_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
					    handle, flags);
	}
#endif

#if SDEI_SUPPORT
	if (is_sdei_fid(smc_fid)) {
		return sdei_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
				flags);
	}
#endif

#if TRNG_SUPPORT
	if (is_trng_fid(smc_fid)) {
		return trng_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
				flags);
	}
#endif /* TRNG_SUPPORT */

#if ERRATA_ABI_SUPPORT
	if (is_errata_fid(smc_fid)) {
		return errata_abi_smc_handler(smc_fid, x1, x2, x3, x4, cookie,
					      handle, flags);
	}
#endif /* ERRATA_ABI_SUPPORT */

#if ENABLE_RME

	if (is_rmmd_el3_fid(smc_fid)) {
		return rmmd_rmm_el3_handler(smc_fid, x1, x2, x3, x4, cookie,
					    handle, flags);
	}

	if (is_rmi_fid(smc_fid)) {
		return rmmd_rmi_handler(smc_fid, x1, x2, x3, x4, cookie,
					handle, flags);
	}
#endif

#if SMC_PCI_SUPPORT
	if (is_pci_fid(smc_fid)) {
		return pci_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
				       flags);
	}
#endif

#if DRTM_SUPPORT
	if (is_drtm_fid(smc_fid)) {
		return drtm_smc_handler(smc_fid, x1, x2, x3, x4, cookie, handle,
					flags);
	}
#endif /* DRTM_SUPPORT */

	switch (smc_fid) {
	case ARM_STD_SVC_CALL_COUNT:
		/*
		 * Return the number of Standard Service Calls. PSCI is the only
		 * standard service implemented; so return number of PSCI calls
		 */
		SMC_RET1(handle, PSCI_NUM_CALLS);

	case ARM_STD_SVC_UID:
		/* Return UID to the caller */
		SMC_UUID_RET(handle, arm_svc_uid);

	case ARM_STD_SVC_VERSION:
		/* Return the version of current implementation */
		SMC_RET2(handle, STD_SVC_VERSION_MAJOR, STD_SVC_VERSION_MINOR);

	default:
		VERBOSE("Unimplemented Standard Service Call: 0x%x \n", smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Register Standard Service Calls as runtime service */
DECLARE_RT_SVC(
		std_svc,

		OEN_STD_START,
		OEN_STD_END,
		SMC_TYPE_FAST,
		std_svc_setup,
		std_svc_smc_handler
);


/*TEE-based PLC*/
static int32_t arm_arc_teeplc_setup(void)
{
	NOTICE("TEEPLC is registering.\n");
	return 0;
}

#define PSEUDO_THREAD_NUM 100
#define PSEUDO_THREAD_SS_SIZE 50
typedef struct pseudo_dynamic_malloc{
	unsigned int thread_id;
	unsigned long shadow_stack[PSEUDO_THREAD_SS_SIZE];
	int shadow_stack_sp;
	int flag;
} pseudo_dynamic_malloc;

static pseudo_dynamic_malloc whole_shadow_stack[PSEUDO_THREAD_NUM]={0};


static int init_thread_local_ss(unsigned int tid){
	for(int i=0;i<PSEUDO_THREAD_NUM;i++){
		if ((whole_shadow_stack[i].thread_id == 0) || \
		((whole_shadow_stack[i].flag == 0) && (whole_shadow_stack[i].shadow_stack_sp == 0))){
		 
		 	whole_shadow_stack[i].thread_id = tid;
		 	whole_shadow_stack[i].flag = 1;
		 	memset(whole_shadow_stack[i].shadow_stack, 0, sizeof(whole_shadow_stack[i].shadow_stack));
		 	return i;
		 }
	}
	//whole_shadow_stack is full.
	return -1;
}

static int find_thread_local_ss(unsigned int tid){
	for(int i=0;i<PSEUDO_THREAD_NUM;i++){
		if (whole_shadow_stack[i].thread_id == tid){
		 	return i;
		 }
	}
	//Cannot find tid ss.
	return -1;
}

static int free_thread_local_ss(unsigned int tid){
	for(int i=0;i<PSEUDO_THREAD_NUM;i++){
		if (whole_shadow_stack[i].thread_id == tid){
		 	whole_shadow_stack[i].flag = 0; 
		 	return i;
		 }
	}
	//Cannot find free tid ss.
	return -1;
}

static int add_lr_thread_local_ss(unsigned int tid, unsigned long lr){
	int find_tid = find_thread_local_ss(tid);
	if(find_tid == -1){
		NOTICE("find ss error!\n");
		return -1;
	}
	int sp = whole_shadow_stack[find_tid].shadow_stack_sp;
	if(sp>=(PSEUDO_THREAD_SS_SIZE-1)){
		NOTICE("shadow_stack too small.\n");
	 	return -2;
	}
	
	whole_shadow_stack[find_tid].shadow_stack[sp] = lr;
	whole_shadow_stack[find_tid].shadow_stack_sp += 1;
	return find_tid;
}

static unsigned long restore_lr_thread_local_ss(unsigned int tid){
	int find_tid = find_thread_local_ss(tid);
	if(find_tid == -1){
		NOTICE("find ss error!\n");
		return -1;
	}
	
	int sp = whole_shadow_stack[find_tid].shadow_stack_sp;
	sp-=1;
	
	if(sp<0){
		NOTICE("shadow_stack is empty.\n");
	 	return -2;
	}
	unsigned long lr = whole_shadow_stack[find_tid].shadow_stack[sp];
	
	whole_shadow_stack[find_tid].shadow_stack_sp -= 1;
	
	return lr;
	
}

static void free_whole_shadow_stack(){
	memset(whole_shadow_stack, 0, sizeof(whole_shadow_stack));
}

static uintptr_t arm_arc_teeplc_smc_handler(uint32_t smc_fid,
    u_register_t x1,
    u_register_t x2,
    u_register_t x3,
    u_register_t x4,
    void *cookie,
    void *handle,
    u_register_t flags)
{
	int ret=0;
//	char *t_usrname;
//	char *t_passwd;
	//static int shadowstack_sp = 0;
	
	//memset(shadowstack, 0, sizeof(shadowstack));
	int tid;
	unsigned long lr;
	switch (x1) {
		case 0:
			NOTICE("Entry in case 0\n");
			int b = x2;
			int c = x3;
			b = b+1;
			c = c+2;
			int d = b*10;
			int e = c*10;
			SMC_RET4(handle,b,c,d,e);
			return 0;
		case 1:
			NOTICE("TEE start add shadow stack.\n");
			tid = x2;
			ret = init_thread_local_ss(tid);
			
			NOTICE("TEE shadow stack creat.\n");
			//SMC_RET1(handle,shadowstack);
			//return 0;
			break;
		case 2:
			NOTICE("TEE start add return address.\n");
			tid = x2;
			lr = x3;
			ret = add_lr_thread_local_ss(tid, lr);

			NOTICE("TEE add return address.\n");
			break;
		case 3:
			NOTICE("TEE start restore return address.\n");
			tid = x2;
			lr = restore_lr_thread_local_ss(tid);

			NOTICE("TEE restore return address.\n");
			SMC_RET2(handle,ret,lr);
			return 0;
		case 4:
			NOTICE("TEE start end shadow stack.\n");
			tid = x2;
			ret = free_thread_local_ss(tid);
			NOTICE("TEE restore return address.\n");
			break;
		case 5:
			free_whole_shadow_stack();
			break;

		default:
			break;
		break;
	}
	NOTICE("TEEPLC is handled. Hello world!!!!\n");
	SMC_RET1(handle, ret);
	return 0;
}	

DECLARE_RT_SVC(
		teeplc,

		OEN_TEEPLC_START,
		OEN_TEEPLC_END,
		SMC_TYPE_FAST,
		arm_arc_teeplc_setup,
		arm_arc_teeplc_smc_handler
);
