
static void (*test_pattern_callback)(char *dst, char *src, int copy_size, int pld_dst);

static void test_instr_NEON(int printlog, int option, int instr_set) 
{ 
    unsigned long copy_size; 
    unsigned long flags; 
    unsigned long i,j,k, pld_dst;
    unsigned long long t1, t2, t_temp, avg_t;
    unsigned long long result_t[10];

    unsigned long temp;
    unsigned long long avg;
    unsigned long long result[10];
    char *dst, *src;
    char output_line[320];
    char buf[32];
    int line_offset = 0;
    dst = &buffer_dst;
    src = &buffer_src;

    dst = (char*)(((unsigned long)(dst) + 0x10) & (~0x7));
    src = (char*)(((unsigned long)(src) + 0x10) & (~0x7));
    
    printk(KERN_EMERG" == MEMCPY PATTERN EVALUATION: 0x%x->0x%x (0x%x->0x%x)\n\r", src, dst, &buffer_src, &buffer_dst);
    if(instr_set == TEST_NEON){
        if(option == TEST_PLD_ONLY){
            test_pattern_callback = neon_pld;
            printk(KERN_EMERG"\n\n\r ==  NEON PLD ONLY ==\n\r");
        }else if(option == TEST_PLD_PLDW){
            test_pattern_callback = neon_pld_pldw;
            printk(KERN_EMERG"\n\n\r ==  NEON PLD PLDW ==\n\r");
        }else if(option == TEST_NOPLD){
            test_pattern_callback = neon_no_pld;
            printk(KERN_EMERG"\n\n\r ==  NEON NO PLD ==\n\r");
        }
    }else if(instr_set == TEST_LDRD){
        if(option == TEST_PLD_ONLY){
            test_pattern_callback = ldrd_pld_only;
            printk(KERN_EMERG"\n\n\r ==  LDRD PLD ONLY ==\n\r");
        }else if(option == TEST_PLD_PLDW){
            test_pattern_callback = ldrd_pld_pldw;
            printk(KERN_EMERG"\n\n\r ==  LDRD PLD  PLDW ==\n\r");
        }else if(option == TEST_NOPLD){
            test_pattern_callback = ldrd_no_pld;
            printk(KERN_EMERG"\n\n\r ==  LDRD NO PLD ==\n\r");
        }
    }else if(instr_set == TEST_LDM){
        if(option == TEST_PLD_ONLY){
            test_pattern_callback = ldm_pld_only;
            printk(KERN_EMERG"\n\n\r ==  LDM PLD ONLY ==\n\r");
        }else if(option == TEST_PLD_PLDW){
            test_pattern_callback = ldm_pld_pldw;
            printk(KERN_EMERG"\n\n\r ==  LDM PLD PLDW ==\n\r");
        }else if(option == TEST_NOPLD){
            test_pattern_callback = ldm_no_pld;
            printk(KERN_EMERG"\n\n\r ==  LDM NO PLD ==\n\r");
        }
    }


    for(i = 0; i< 256 + 8 + 4; i++ )
    {
        line_offset = 0;
        memset(output_line, 0, 320);
        /* inc 512 byte, 512~128K*/
        /* inc inc 128K, 128KB ~ 1MB */
        /* inc 256K, 1MB ~ 4MB */
        if(i<256){
            copy_size = 512 + i*512; 
        }else if (i< 256 + 8){
            copy_size = 1024*128 + (i-256)*1024*128; 
        }else if (i< 256 + 8 + 4){
            copy_size = 1024*1024 + (i-256-8)*1024*1024;
        }

        mdelay(DELAY_MS);
        inner_dcache_flush_all();
        if(printlog == 1){
            memset(buf, 0, 32);
            sprintf(buf," %lu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }
        avg_t = 0;
        avg = 0;

#if 0
        preempt_disable();
        local_irq_save(flags);
        /* no pld */
        for(j = 0; j<8; j++){
            mdelay(DELAY_MS);
            if(flush_cache == 1)
                inner_dcache_flush_all();
                /* get timer */
#ifdef TIME_COUNT
            t1 = sched_clock();
#else
            temp = 0;

            /* enable ARM CPU PMU */
            asm volatile(
                    "mov %0, #0\n"
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );

#endif
            /* main test*/
            neon_no_pld(dst, src, copy_size, pld_dst);
#ifdef TIME_COUNT
            t2 = sched_clock();
            result_t[j] = nsec_low(t2 - t1);
#else
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            result[j] = temp;
#endif
/*----*/
        }// 10 times loop
        local_irq_restore(flags); 
        preempt_enable();

#ifdef TIME_COUNT
        for(j = 0; j<8; j++){
            avg_t += result_t[j];
        }
        avg_t = avg_t >>3;
#else
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >> 3;
#endif
        if(printlog == 1){
            //printk(KERN_CONT" %d ", avg );
            memset(buf, 0, 32);
#ifdef TIME_COUNT
            snprintf(buf,32, " %llu ", avg_t);
#else
            snprintf(buf,32, " %llu ",avg);
#endif
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
            line_offset += strlen(buf);
        }
#endif

        for(pld_dst = PLD_START_DST; pld_dst<CACHELINE_SIZE*PLD_END_DST; pld_dst+= CACHELINE_SIZE * PLD_DST_INC){
            avg_t = 0;
            avg = 0;
            preempt_disable();
            local_irq_save(flags);
            for(j = 0; j<8; j++){
                mdelay(DELAY_MS);
                if(flush_cache == 1)
                    inner_dcache_flush_all();
                for(k=0; k<2;k++){
                    if(k==0)
                        inner_dcache_flush_all();

#ifdef TIME_COUNT 
                    t1 = sched_clock();
#else
            temp = 0;

            /* enable ARM CPU PMU */
            asm volatile(
                    "mov %0, #0\n"
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
#endif
                    test_pattern_callback(dst, src, copy_size, pld_dst);
#ifdef TIME_COUNT 
                    t2 = sched_clock();
                    result_t[j] = nsec_low(t2 - t1);
#else
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            result[j] = temp;
#endif
                }
            }// 10 times loop
            local_irq_restore(flags); 
            preempt_enable();

#ifdef TIME_COUNT 
        for(j = 0; j<8; j++){
            avg_t += result_t[j];
        }
        avg_t = avg_t >>3;
#else
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >> 3;
#endif
            if(printlog == 1){
                memset(buf, 0, 32);
#ifdef TIME_COUNT 
                snprintf(buf,32, " %llu ", avg_t);
#else
                snprintf(buf,32, " %llu ",avg);
#endif
                snprintf(output_line+line_offset, 320 - line_offset, "%s", buf);
                line_offset += strlen(buf);
            }
        }//pld dist loop
        if(printlog == 1)
            printk(KERN_EMERG"%s\n", output_line);
    }
    if(instr_set == TEST_NEON){
        if(option == TEST_PLD_ONLY){
            printk(KERN_EMERG"\n\r ==  NEON PLD ONLY test Done(%pS) == flush_cache:%d\n\r", (void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_PLD_PLDW){
            printk(KERN_EMERG"\n\r ==  NEON PLD PLDW test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_NOPLD){
            printk(KERN_EMERG"\n\r ==  NEON NOPLD test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }
    }else if(instr_set == TEST_LDRD){
        if(option == TEST_PLD_ONLY){
            printk(KERN_EMERG"\n\r ==  LDRD PLD only test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_PLD_PLDW){
            printk(KERN_EMERG"\n\r ==  LDRD PLD PLDW test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_NOPLD){
            printk(KERN_EMERG"\n\r ==  LDRD NOPLD test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }
    }else if(instr_set == TEST_LDM){
        if(option == TEST_PLD_ONLY){
            printk(KERN_EMERG"\n\r ==  LDM PLD only test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_PLD_PLDW){
            printk(KERN_EMERG"\n\r ==  LDM PLD PLDW test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }else if(option == TEST_NOPLD){
            printk(KERN_EMERG"\n\r ==  LDM NOPLD test Done(%pS) == flush_cache:%d\n\r",(void*)test_pattern_callback, flush_cache);
        }
    }
}
