void neon_pld(char *dst, char *src, int copy_size, int pld_dst)
{

    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"

            "subs        r2, r2, #64\n"
            "1:\n" 
            // The main loop copies 64 bytes at a time.
            ".word 0xf421020d\n"//"vld1.8      {d0  - d3},   [r1]!\n"
            ".word 0xf421420d\n"//"vld1.8      {d4  - d7},   [r1]!\n"
            ".word 0xf5d1f100\n" //"pld         [r1, r3]\n"
            "subs        r2, r2, #64\n"
            ".word 0xf400022d\n"//"vst1.8      {d0  - d3},   [r0, :128]!\n"
            ".word 0xf400422d\n"//"vst1.8      {d4  - d7},   [r0, :128]!\n"
            "bge         1b\n"

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2, r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst) 
            :"r0","r1","r2","r3"
              );

} 
void neon_pld_pldw(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"

            "subs        r2, r2, #64\n"
            "1:\n" 
            // The main loop copies 64 bytes at a time.
            ".word 0xf421020d\n"//"vld1.8      {d0  - d3},   [r1]!\n"
            ".word 0xf421420d\n"//"vld1.8      {d4  - d7},   [r1]!\n"
            ".word 0xf5d1f100\n" //"pld         [r1, r3]\n"
            "subs        r2, r2, #64\n"
            ".word 0xf400022d\n"//"vst1.8      {d0  - d3},   [r0, :128]!\n"
            ".word 0xf400422d\n"//"vst1.8      {d4  - d7},   [r0, :128]!\n"
            ".word 0xf590f100\n" //pldw
            "bge         1b\n"

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2, r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst) 
            :"r0","r1","r2","r3"
              );


} 
void neon_no_pld(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"

            "subs        r2, r2, #64\n"
            "1:\n" 
            // The main loop copies 64 bytes at a time.
            ".word 0xf421020d\n"//"vld1.8      {d0  - d3},   [r1]!\n"
            ".word 0xf421420d\n"//"vld1.8      {d4  - d7},   [r1]!\n"
            "subs        r2, r2, #64\n"
            ".word 0xf400022d\n"//"vst1.8      {d0  - d3},   [r0, :128]!\n"
            ".word 0xf400422d\n"//"vst1.8      {d4  - d7},   [r0, :128]!\n"
            "bge         1b\n"

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2, r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst) 
            :"r0","r1","r2","r3"
              );

} 

void ldrd_pld_only(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r3}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"
            "stmfd        sp!, {r4, r5, r6, r7, r8, r9}\n" 
            "1:\n" 
            //"pldw        [r0, r9]\n" 
            //".word 0xf790f009 \n"
            "ldrd    r4, r5, [r1, #0]\n"
            "strd    r4, r5, [r0, #0]\n"
            "ldrd    r4, r5, [r1, #8]\n"
            "strd    r4, r5, [r0, #8]\n"
            "ldrd    r4, r5, [r1, #16]\n"
            "strd    r4, r5, [r0, #16]\n"
            "ldrd    r4, r5, [r1, #24]\n"
            "strd    r4, r5, [r0, #24]\n"
            "ldrd    r4, r5, [r1, #32]\n"
            "strd    r4, r5, [r0, #32]\n"

            "ldrd    r4, r5, [r1, #40]\n"
            "ldrd    r6, r7, [r1, #48]\n"
            "ldrd    r8, r9, [r1, #56]\n"
            "pld         [r1, r9]\n"
            "strd    r4, r5, [r0, #40]\n"
            "strd    r6, r7, [r0, #48]\n"
            "strd    r8, r9, [r0, #56]\n"
            " add     r0, r0, #64\n"
            " add     r1, r1, #64\n"
            " subs    r2, r2, #64\n"
            " bge     1b\n"

            "ldmfd       sp!, { r4, r5, r6, r7, r8, r9}\n"
            "pop {r0,r1,r2,r3}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r3"
              );
}
void ldrd_pld_pldw(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r3}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"
            "stmfd        sp!, {r4, r5, r6, r7, r8, r9}\n" 
            "1:\n" 
            ".word 0xf790f009 \n"//"pldw        [r0, r9]\n" 
            "ldrd    r4, r5, [r1, #0]\n"
            "strd    r4, r5, [r0, #0]\n"
            "ldrd    r4, r5, [r1, #8]\n"
            "strd    r4, r5, [r0, #8]\n"
            "ldrd    r4, r5, [r1, #16]\n"
            "strd    r4, r5, [r0, #16]\n"
            "ldrd    r4, r5, [r1, #24]\n"
            "strd    r4, r5, [r0, #24]\n"
            "ldrd    r4, r5, [r1, #32]\n"
            "strd    r4, r5, [r0, #32]\n"

            "ldrd    r4, r5, [r1, #40]\n"
            "ldrd    r6, r7, [r1, #48]\n"
            "ldrd    r8, r9, [r1, #56]\n"
            "pld         [r1, r9]\n"
            "strd    r4, r5, [r0, #40]\n"
            "strd    r6, r7, [r0, #48]\n"
            "strd    r8, r9, [r0, #56]\n"
            " add     r0, r0, #64\n"
            " add     r1, r1, #64\n"
            " subs    r2, r2, #64\n"
            " bge     1b\n"

            "ldmfd       sp!, { r4, r5, r6, r7, r8, r9}\n"
            "pop {r0,r1,r2,r3}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r3"
              );
}
void ldrd_no_pld(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r3}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r3, %3\n"
            "stmfd        sp!, {r4, r5, r6, r7, r8, r9}\n" 
            "1:\n" 
            //"pldw        [r0, r9]\n" 
            //".word 0xf790f009 \n"
            "ldrd    r4, r5, [r1, #0]\n"
            "strd    r4, r5, [r0, #0]\n"
            "ldrd    r4, r5, [r1, #8]\n"
            "strd    r4, r5, [r0, #8]\n"
            "ldrd    r4, r5, [r1, #16]\n"
            "strd    r4, r5, [r0, #16]\n"
            "ldrd    r4, r5, [r1, #24]\n"
            "strd    r4, r5, [r0, #24]\n"
            "ldrd    r4, r5, [r1, #32]\n"
            "strd    r4, r5, [r0, #32]\n"

            "ldrd    r4, r5, [r1, #40]\n"
            "ldrd    r6, r7, [r1, #48]\n"
            "ldrd    r8, r9, [r1, #56]\n"
            //"pld         [r1, r9]\n"
            "strd    r4, r5, [r0, #40]\n"
            "strd    r6, r7, [r0, #48]\n"
            "strd    r8, r9, [r0, #56]\n"
            " add     r0, r0, #64\n"
            " add     r1, r1, #64\n"
            " subs    r2, r2, #64\n"
            " bge     1b\n"

            "ldmfd       sp!, { r4, r5, r6, r7, r8, r9}\n"
            "pop {r0,r1,r2,r3}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r3"
              );
}

void ldm_pld_only(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r9, %3\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 

            "1:\n" 
            "pld         [r1, r9]\n"
            //"pldw        [r0, r9]\n" 
            //".word 0xf790f009 \n"
            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "blt 2f\n"

            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "bge 1b\n"
            "2:\n" 

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2,r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r9"
              );
}
void ldm_pld_pldw(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r9, %3\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 

            "1:\n" 
            "pld         [r1, r9]\n"
            ".word 0xf790f009 \n"//"pldw        [r0, r9]\n" 
            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "blt 2f\n"

            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "bge 1b\n"
            "2:\n" 

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2,r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r9"
              );
}
void ldm_no_pld(char *dst, char *src, int copy_size, int pld_dst)
{
    asm volatile(
            "push {r0,r1,r2,r9}\n"
            "mov r0, %0\n"
            "mov r1, %1\n"
            "mov r2, %2\n"
            "mov r9, %3\n"
            "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 

            "1:\n" 
            //"pld         [r1, r9]\n"
            //"pldw        [r0, r9]\n" 
            //".word 0xf790f009 \n"
            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "blt 2f\n"

            "ldmia       r1!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "subs        r2, r2, #32  \n"
            "stmia       r0!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "bge 1b\n"
            "2:\n" 

            "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
            "pop {r0,r1,r2,r9}\n"
            :: "r"(dst), "r"(src), "r"(copy_size), "r"(pld_dst)
            :"r0","r1","r2","r9"
              );
}
