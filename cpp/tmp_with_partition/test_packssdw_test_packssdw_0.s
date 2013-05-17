# BB#11:                                # %test_packssdw_0.v1_loop.preheader
	cmpl	$0, -16(%rsp)           # 4-byte Folded Reload
	jle	.LBB97_16
# BB#12:                                # %test_packssdw_0.v0.v0_loop.preheader.us.preheader
	movl	-36(%rsp), %eax         # 4-byte Reload
	leal	7(%rax), %eax
	subl	-52(%rsp), %eax         # 4-byte Folded Reload
	movl	%eax, -44(%rsp)         # 4-byte Spill
	movl	$7, %ecx
	movl	-40(%rsp), %eax         # 4-byte Reload
	.align	16, 0x90
.LBB97_15:                              # %test_packssdw_0.v0.v0_loop.preheader.us
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB97_13 Depth 2
	movl	%eax, -32(%rsp)         # 4-byte Spill
	movl	%ecx, -28(%rsp)         # 4-byte Spill
	movl	-44(%rsp), %eax         # 4-byte Reload
                                        # kill: EAX<def> EAX<kill> RAX<def>
	movl	%ecx, %r9d
	movl	-16(%rsp), %r11d        # 4-byte Reload
	.align	16, 0x90
.LBB97_13:                              # %test_packssdw_0.v0.v0_loop.us
                                        #   Parent Loop BB97_15 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-3(%rax), %ecx
	movslq	%ecx, %rcx
	movd	(%r12,%rcx,4), %xmm1
	leal	-2(%rax), %ecx
	movslq	%ecx, %rcx
	leal	-7(%rax), %edx
	movslq	%edx, %rdx
	movd	(%r12,%rdx,4), %xmm0
	pinsrd	$1, (%r12,%rcx,4), %xmm1
	leal	-1(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm1
	leal	-6(%rax), %ecx
	movslq	%ecx, %rcx
	movslq	%eax, %rax
	pinsrd	$3, (%r12,%rax,4), %xmm1
	pinsrd	$1, (%r12,%rcx,4), %xmm0
	leal	-5(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm0
	leal	-4(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$3, (%r12,%rcx,4), %xmm0
	packssdw	%xmm1, %xmm0
	addl	$8, %eax
	leal	-7(%r9), %ecx
	movslq	%ecx, %r15
	decl	%r11d
	movslq	%r9d, %rcx
	movq	%rcx, -8(%rsp)          # 8-byte Spill
	leal	-3(%r9), %esi
	leal	-2(%r9), %ebp
	leal	-1(%r9), %edi
	leal	8(%r9), %ecx
	movl	%ecx, -12(%rsp)         # 4-byte Spill
	leal	-6(%r9), %ebx
	leal	-5(%r9), %ecx
	leal	-4(%r9), %edx
	movslq	%edi, %r9
	movslq	%ebp, %rbp
	movslq	%esi, %rsi
	movslq	%edx, %r13
	movslq	%ecx, %r10
	movslq	%ebx, %r8
	movd	%xmm0, %ecx
	pextrw	$7, %xmm0, %ebx
	movw	%cx, (%r14,%r15,2)
	pextrw	$1, %xmm0, %ecx
	movw	%cx, (%r14,%r8,2)
	pextrw	$2, %xmm0, %ecx
	movw	%cx, (%r14,%r10,2)
	pextrw	$3, %xmm0, %ecx
	movw	%cx, (%r14,%r13,2)
	pextrw	$4, %xmm0, %ecx
	movw	%cx, (%r14,%rsi,2)
	pextrw	$5, %xmm0, %ecx
	movw	%cx, (%r14,%rbp,2)
	pextrw	$6, %xmm0, %ecx
	movw	%cx, (%r14,%r9,2)
	movq	-8(%rsp), %rcx          # 8-byte Reload
	movw	%bx, (%r14,%rcx,2)
	movl	-12(%rsp), %r9d         # 4-byte Reload
	jne	.LBB97_13
# BB#14:                                # %test_packssdw_0.v0.v0_after_loop.us
# BB#17:                                # %test_packssdw_0.v1_loop12.preheader
	cmpl	$0, -16(%rsp)           # 4-byte Folded Reload
	jle	.LBB97_22
# BB#18:                                # %test_packssdw_0.v0.v0_loop15.preheader.us.preheader
	movl	-48(%rsp), %eax         # 4-byte Reload
	notl	%eax
	movl	$-6, %ecx
	cmpl	$-7, %eax
	cmovgl	%eax, %ecx
	notl	%ecx
	imull	-20(%rsp), %ecx         # 4-byte Folded Reload
	addl	$7, %ecx
	movl	-36(%rsp), %eax         # 4-byte Reload
	leal	7(%rax), %eax
	subl	-52(%rsp), %eax         # 4-byte Folded Reload
	movl	%eax, -32(%rsp)         # 4-byte Spill
	.align	16, 0x90
.LBB97_21:                              # %test_packssdw_0.v0.v0_loop15.preheader.us
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB97_19 Depth 2
	movl	%ecx, -28(%rsp)         # 4-byte Spill
	movl	-32(%rsp), %eax         # 4-byte Reload
                                        # kill: EAX<def> EAX<kill> RAX<def>
	movl	%ecx, %ebx
	movl	-16(%rsp), %r13d        # 4-byte Reload
	.align	16, 0x90
.LBB97_19:                              # %test_packssdw_0.v0.v0_loop15.us
                                        #   Parent Loop BB97_21 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-3(%rax), %ecx
	movslq	%ecx, %rcx
	movd	(%r12,%rcx,4), %xmm1
	leal	-2(%rax), %ecx
	movslq	%ecx, %rcx
	leal	-7(%rax), %edx
	movslq	%edx, %rdx
	movd	(%r12,%rdx,4), %xmm0
	pinsrd	$1, (%r12,%rcx,4), %xmm1
	leal	-1(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm1
	leal	-6(%rax), %ecx
	movslq	%ecx, %rcx
	movslq	%eax, %rax
	pinsrd	$3, (%r12,%rax,4), %xmm1
	pinsrd	$1, (%r12,%rcx,4), %xmm0
	leal	-5(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm0
	leal	-4(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$3, (%r12,%rcx,4), %xmm0
	packssdw	%xmm1, %xmm0
	addl	$8, %eax
	leal	-7(%rbx), %ecx
	movslq	%ecx, %r9
	decl	%r13d
	movslq	%ebx, %rcx
	movq	%rcx, -8(%rsp)          # 8-byte Spill
	leal	-3(%rbx), %ebp
	leal	-2(%rbx), %esi
	leal	-1(%rbx), %edx
	leal	8(%rbx), %r11d
	leal	-6(%rbx), %ecx
	leal	-5(%rbx), %edi
	leal	-4(%rbx), %ebx
	movslq	%edx, %r8
	movslq	%esi, %rdx
	movslq	%ebp, %rsi
	movslq	%ebx, %r15
	movslq	%edi, %rbp
	movslq	%ecx, %rcx
	movd	%xmm0, %edi
	pextrw	$7, %xmm0, %r10d
	movw	%di, (%r14,%r9,2)
	pextrw	$1, %xmm0, %edi
	movw	%di, (%r14,%rcx,2)
	pextrw	$2, %xmm0, %ecx
	movw	%cx, (%r14,%rbp,2)
	pextrw	$3, %xmm0, %ecx
	movw	%cx, (%r14,%r15,2)
	pextrw	$4, %xmm0, %ecx
	movw	%cx, (%r14,%rsi,2)
	pextrw	$5, %xmm0, %ecx
	movw	%cx, (%r14,%rdx,2)
	pextrw	$6, %xmm0, %ecx
	movw	%cx, (%r14,%r8,2)
	movq	-8(%rsp), %rcx          # 8-byte Reload
	movw	%r10w, (%r14,%rcx,2)
	movl	%r11d, %ebx
	jne	.LBB97_19
# BB#20:                                # %test_packssdw_0.v0.v0_after_loop22.us
# BB#23:                                # %test_packssdw_0.v1_loop26.preheader
	cmpl	$0, -16(%rsp)           # 4-byte Folded Reload
	jle	.LBB97_28
# BB#24:                                # %test_packssdw_0.v0.v0_loop29.preheader.us.preheader
	movl	-48(%rsp), %esi         # 4-byte Reload
	movl	%esi, %ecx
	notl	%ecx
	movl	$-6, %eax
	cmpl	$-7, %ecx
	cmovgl	%ecx, %eax
	leal	-4(%rsi,%rax), %ecx
	xorl	%esi, %esi
	testl	%ecx, %ecx
	cmovnsl	%ecx, %esi
	addl	$7, %edx
	subl	-52(%rsp), %edx         # 4-byte Folded Reload
	movl	%edx, -36(%rsp)         # 4-byte Spill
	decl	%esi
	subl	%eax, %esi
	imull	-20(%rsp), %esi         # 4-byte Folded Reload
	addl	$7, %esi
	.align	16, 0x90
.LBB97_27:                              # %test_packssdw_0.v0.v0_loop29.preheader.us
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB97_25 Depth 2
	movl	%esi, -24(%rsp)         # 4-byte Spill
	movl	%edx, %eax
	movl	%esi, %ebx
	movl	-16(%rsp), %r10d        # 4-byte Reload
	.align	16, 0x90
.LBB97_25:                              # %test_packssdw_0.v0.v0_loop29.us
                                        #   Parent Loop BB97_27 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leal	-3(%rax), %ecx
	movslq	%ecx, %rcx
	movd	(%r12,%rcx,4), %xmm1
	leal	-2(%rax), %ecx
	movslq	%ecx, %rcx
	leal	-7(%rax), %edx
	movslq	%edx, %rdx
	movd	(%r12,%rdx,4), %xmm0
	pinsrd	$1, (%r12,%rcx,4), %xmm1
	leal	-1(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm1
	leal	-6(%rax), %ecx
	movslq	%ecx, %rcx
	movslq	%eax, %rax
	pinsrd	$3, (%r12,%rax,4), %xmm1
	pinsrd	$1, (%r12,%rcx,4), %xmm0
	leal	-5(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$2, (%r12,%rcx,4), %xmm0
	leal	-4(%rax), %ecx
	movslq	%ecx, %rcx
	pinsrd	$3, (%r12,%rcx,4), %xmm0
	packssdw	%xmm1, %xmm0
	addl	$8, %eax
	leal	-7(%rbx), %ecx
	movslq	%ecx, %r13
	decl	%r10d
	movslq	%ebx, %rcx
	movq	%rcx, -8(%rsp)          # 8-byte Spill
	leal	-3(%rbx), %ecx
	leal	-2(%rbx), %esi
	leal	-1(%rbx), %edx
	leal	8(%rbx), %r11d
	leal	-6(%rbx), %ebp
	leal	-5(%rbx), %edi
	leal	-4(%rbx), %ebx
	movslq	%edx, %r8
	movslq	%esi, %r15
	movslq	%ecx, %r9
	movslq	%ebx, %rbx
	movslq	%edi, %rcx
	movslq	%ebp, %rsi
	movd	%xmm0, %edi
	pextrw	$7, %xmm0, %edx
	movw	%di, (%r14,%r13,2)
	pextrw	$1, %xmm0, %edi
	movw	%di, (%r14,%rsi,2)
	pextrw	$2, %xmm0, %esi
	movw	%si, (%r14,%rcx,2)
	pextrw	$3, %xmm0, %ecx
	movw	%cx, (%r14,%rbx,2)
	pextrw	$4, %xmm0, %ecx
	movw	%cx, (%r14,%r9,2)
	pextrw	$5, %xmm0, %ecx
	movw	%cx, (%r14,%r15,2)
	pextrw	$6, %xmm0, %ecx
	movw	%cx, (%r14,%r8,2)
	movq	-8(%rsp), %rcx          # 8-byte Reload
	movw	%dx, (%r14,%rcx,2)
	movl	%r11d, %ebx
	jne	.LBB97_25
# BB#26:                                # %test_packssdw_0.v0.v0_after_loop30.us
