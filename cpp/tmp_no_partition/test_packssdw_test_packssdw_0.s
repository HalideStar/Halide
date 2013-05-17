# BB#11:                                # %test_packssdw_0.v1_loop.preheader
	sarl	$3, %r8d
	testl	%r8d, %r8d
	jle	.LBB97_16
# BB#12:                                # %test_packssdw_0.v0.v0_loop.preheader.us.preheader
	movl	40(%rax), %r10d
	movq	(%rax), %rax
	movq	(%rcx), %rcx
	subl	%edi, %r9d
	xorl	%edi, %edi
	.align	16, 0x90
.LBB97_15:                              # %test_packssdw_0.v0.v0_loop.preheader.us
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB97_13 Depth 2
	movl	%r9d, %esi
	movl	%edi, %ebx
	movl	%r8d, %ebp
	.align	16, 0x90
.LBB97_13:                              # %test_packssdw_0.v0.v0_loop.us
                                        #   Parent Loop BB97_15 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movslq	%esi, %rsi
	movdqu	16(%rcx,%rsi,4), %xmm1
	movdqu	(%rcx,%rsi,4), %xmm0
	packssdw	%xmm1, %xmm0
	movslq	%ebx, %rbx
	movdqu	%xmm0, (%rax,%rbx,2)
	addl	$8, %esi
	addl	$8, %ebx
	decl	%ebp
	jne	.LBB97_13
# BB#14:                                # %test_packssdw_0.v0.v0_after_loop.us
