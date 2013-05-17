	.file	"halide_f2"
	.section	.text.halide_set_custom_allocator,"axG",@progbits,halide_set_custom_allocator,comdat
	.weak	halide_set_custom_allocator
	.align	16, 0x90
	.type	halide_set_custom_allocator,@function
halide_set_custom_allocator:            # @halide_set_custom_allocator
	.cfi_startproc
# BB#0:
	movq	halide_custom_malloc@GOTPCREL(%rip), %rax
	movq	%rdi, (%rax)
	movq	halide_custom_free@GOTPCREL(%rip), %rax
	movq	%rsi, (%rax)
	ret
.Ltmp0:
	.size	halide_set_custom_allocator, .Ltmp0-halide_set_custom_allocator
	.cfi_endproc

	.section	.text.halide_malloc,"axG",@progbits,halide_malloc,comdat
	.weak	halide_malloc
	.align	16, 0x90
	.type	halide_malloc,@function
halide_malloc:                          # @halide_malloc
	.cfi_startproc
# BB#0:
	subq	$24, %rsp
.Ltmp2:
	.cfi_def_cfa_offset 32
	movq	halide_custom_malloc@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	testq	%rax, %rax
	je	.LBB1_1
# BB#2:
	addq	$24, %rsp
	jmpq	*%rax  # TAILCALL
.LBB1_1:
	addq	$32, %rdi
	callq	malloc@PLT
	leaq	32(%rax), %rcx
	andq	$-32, %rcx
	movq	%rax, -8(%rcx)
	movq	%rcx, %rax
	addq	$24, %rsp
	ret
.Ltmp3:
	.size	halide_malloc, .Ltmp3-halide_malloc
	.cfi_endproc

	.section	.text.halide_free,"axG",@progbits,halide_free,comdat
	.weak	halide_free
	.align	16, 0x90
	.type	halide_free,@function
halide_free:                            # @halide_free
	.cfi_startproc
# BB#0:
	movq	halide_custom_free@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	testq	%rax, %rax
	je	.LBB2_2
# BB#1:
	jmpq	*%rax  # TAILCALL
.LBB2_2:
	movq	-8(%rdi), %rdi
	jmp	free@PLT                # TAILCALL
.Ltmp4:
	.size	halide_free, .Ltmp4-halide_free
	.cfi_endproc

	.section	.text.halide_start_clock,"axG",@progbits,halide_start_clock,comdat
	.weak	halide_start_clock
	.align	16, 0x90
	.type	halide_start_clock,@function
halide_start_clock:                     # @halide_start_clock
	.cfi_startproc
# BB#0:
	subq	$24, %rsp
.Ltmp6:
	.cfi_def_cfa_offset 32
	movq	halide_reference_clock@GOTPCREL(%rip), %rdi
	xorl	%esi, %esi
	callq	gettimeofday@PLT
	xorl	%eax, %eax
	addq	$24, %rsp
	ret
.Ltmp7:
	.size	halide_start_clock, .Ltmp7-halide_start_clock
	.cfi_endproc

	.section	.text.halide_current_time,"axG",@progbits,halide_current_time,comdat
	.weak	halide_current_time
	.align	16, 0x90
	.type	halide_current_time,@function
halide_current_time:                    # @halide_current_time
	.cfi_startproc
# BB#0:
	subq	$24, %rsp
.Ltmp9:
	.cfi_def_cfa_offset 32
	leaq	8(%rsp), %rdi
	xorl	%esi, %esi
	callq	gettimeofday@PLT
	movq	halide_reference_clock@GOTPCREL(%rip), %rsi
	movq	8(%rsp), %rcx
	movq	16(%rsp), %rax
	subq	8(%rsi), %rax
	movabsq	$2361183241434822607, %rdx # imm = 0x20C49BA5E353F7CF
	imulq	%rdx
	movq	%rdx, %rax
	shrq	$63, %rax
	shrq	$7, %rdx
	addl	%eax, %edx
	subq	(%rsi), %rcx
	movabsq	$4294967296000, %rax    # imm = 0x3E800000000
	imulq	%rcx, %rax
	shrq	$32, %rax
	addl	%edx, %eax
                                        # kill: EAX<def> EAX<kill> RAX<kill>
	addq	$24, %rsp
	ret
.Ltmp10:
	.size	halide_current_time, .Ltmp10-halide_current_time
	.cfi_endproc

	.section	.text.halide_printf,"axG",@progbits,halide_printf,comdat
	.weak	halide_printf
	.align	16, 0x90
	.type	halide_printf,@function
halide_printf:                          # @halide_printf
	.cfi_startproc
# BB#0:
	subq	$216, %rsp
.Ltmp12:
	.cfi_def_cfa_offset 224
	movq	%rdi, %r10
	testb	%al, %al
	je	.LBB5_2
# BB#1:
	movaps	%xmm0, 64(%rsp)
	movaps	%xmm1, 80(%rsp)
	movaps	%xmm2, 96(%rsp)
	movaps	%xmm3, 112(%rsp)
	movaps	%xmm4, 128(%rsp)
	movaps	%xmm5, 144(%rsp)
	movaps	%xmm6, 160(%rsp)
	movaps	%xmm7, 176(%rsp)
.LBB5_2:
	movq	%r9, 56(%rsp)
	movq	%r8, 48(%rsp)
	movq	%rcx, 40(%rsp)
	movq	%rdx, 32(%rsp)
	movq	%rsi, 24(%rsp)
	leaq	16(%rsp), %rax
	movq	%rax, 208(%rsp)
	leaq	224(%rsp), %rax
	movq	%rax, 200(%rsp)
	movl	$48, 196(%rsp)
	movl	$8, 192(%rsp)
	movq	stderr@GOTPCREL(%rip), %rax
	movq	(%rax), %rdi
	leaq	192(%rsp), %rdx
	movq	%r10, %rsi
	callq	vfprintf@PLT
	addq	$216, %rsp
	ret
.Ltmp13:
	.size	halide_printf, .Ltmp13-halide_printf
	.cfi_endproc

	.section	.text.halide_error,"axG",@progbits,halide_error,comdat
	.weak	halide_error
	.align	16, 0x90
	.type	halide_error,@function
halide_error:                           # @halide_error
	.cfi_startproc
# BB#0:
	subq	$24, %rsp
.Ltmp15:
	.cfi_def_cfa_offset 32
	movq	%rdi, %rax
	movq	halide_error_handler@GOTPCREL(%rip), %rcx
	movq	(%rcx), %rcx
	testq	%rcx, %rcx
	je	.LBB6_2
# BB#1:
	movq	%rax, %rdi
	addq	$24, %rsp
	jmpq	*%rcx  # TAILCALL
.LBB6_2:
	leaq	.L.str(%rip), %rdi
	movq	%rax, %rsi
	xorb	%al, %al
	callq	halide_printf@PLT
	movl	$1, %edi
	addq	$24, %rsp
	jmp	exit@PLT                # TAILCALL
.Ltmp16:
	.size	halide_error, .Ltmp16-halide_error
	.cfi_endproc

	.section	.text.halide_set_error_handler,"axG",@progbits,halide_set_error_handler,comdat
	.weak	halide_set_error_handler
	.align	16, 0x90
	.type	halide_set_error_handler,@function
halide_set_error_handler:               # @halide_set_error_handler
	.cfi_startproc
# BB#0:
	movq	halide_error_handler@GOTPCREL(%rip), %rax
	movq	%rdi, (%rax)
	ret
.Ltmp17:
	.size	halide_set_error_handler, .Ltmp17-halide_set_error_handler
	.cfi_endproc

	.section	.text.halide_debug_to_file,"axG",@progbits,halide_debug_to_file,comdat
	.weak	halide_debug_to_file
	.align	16, 0x90
	.type	halide_debug_to_file,@function
halide_debug_to_file:                   # @halide_debug_to_file
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Ltmp25:
	.cfi_def_cfa_offset 16
	pushq	%r15
.Ltmp26:
	.cfi_def_cfa_offset 24
	pushq	%r14
.Ltmp27:
	.cfi_def_cfa_offset 32
	pushq	%r13
.Ltmp28:
	.cfi_def_cfa_offset 40
	pushq	%r12
.Ltmp29:
	.cfi_def_cfa_offset 48
	pushq	%rbx
.Ltmp30:
	.cfi_def_cfa_offset 56
	subq	$296, %rsp              # imm = 0x128
.Ltmp31:
	.cfi_def_cfa_offset 352
.Ltmp32:
	.cfi_offset %rbx, -56
.Ltmp33:
	.cfi_offset %r12, -48
.Ltmp34:
	.cfi_offset %r13, -40
.Ltmp35:
	.cfi_offset %r14, -32
.Ltmp36:
	.cfi_offset %r15, -24
.Ltmp37:
	.cfi_offset %rbp, -16
	movl	%r9d, %r13d
	movl	%r8d, %r14d
	movl	%ecx, %ebp
	movl	%edx, %r15d
	movq	%rsi, 32(%rsp)          # 8-byte Spill
	movq	%rdi, %rbx
	leaq	.L.str1(%rip), %rsi
	callq	fopen@PLT
	movq	%rax, %r12
	movl	$-1, %eax
	testq	%r12, %r12
	je	.LBB8_22
# BB#1:
	movl	%r14d, %eax
	imull	%ebp, %eax
	imull	%r13d, %eax
	movslq	%eax, %rax
	movslq	%r15d, %rcx
	imulq	%rax, %rcx
	movq	%rcx, 40(%rsp)          # 8-byte Spill
	movq	%rbx, %rdi
	movl	$46, %esi
	callq	strrchr@PLT
	movq	%rax, %rbx
	testq	%rbx, %rbx
	je	.LBB8_19
# BB#2:
	movsbl	1(%rbx), %edi
	callq	tolower@PLT
	cmpl	$116, %eax
	jne	.LBB8_19
# BB#3:
	movsbl	2(%rbx), %edi
	callq	tolower@PLT
	cmpl	$105, %eax
	jne	.LBB8_19
# BB#4:
	movsbl	3(%rbx), %edi
	callq	tolower@PLT
	cmpl	$102, %eax
	jne	.LBB8_19
# BB#5:
	movsbl	4(%rbx), %edi
	callq	tolower@PLT
	testl	%eax, %eax
	je	.LBB8_8
# BB#6:
	movsbl	4(%rbx), %edi
	callq	tolower@PLT
	cmpl	$102, %eax
	jne	.LBB8_19
# BB#7:                                 # %_ZN12_GLOBAL__N_118has_tiff_extensionEPKc.exit.i
	movsbl	5(%rbx), %edi
	callq	tolower@PLT
	testl	%eax, %eax
	je	.LBB8_8
.LBB8_19:                               # %_ZN12_GLOBAL__N_118has_tiff_extensionEPKc.exit.thread.i
	movl	%r15d, 48(%rsp)
	movl	%ebp, 52(%rsp)
	movl	%r14d, 56(%rsp)
	movl	%r13d, 60(%rsp)
	movl	352(%rsp), %eax
	movl	%eax, 64(%rsp)
	leaq	48(%rsp), %rdi
	movl	$20, %esi
	movl	$1, %edx
	movq	%r12, %rcx
	callq	fwrite@PLT
	movl	$-2, %r14d
	cmpl	$1, %eax
	jne	.LBB8_21
	jmp	.LBB8_20
.LBB8_8:                                # %_ZN12_GLOBAL__N_118has_tiff_extensionEPKc.exit.thread1.i
	cmpl	$5, %r14d
	setl	%cl
	cmpl	$2, %r13d
	setb	%al
	andb	%cl, %al
	cmovnel	%r14d, %r13d
	movw	$18761, 80(%rsp)        # imm = 0x4949
	movw	$42, 82(%rsp)
	movl	$8, 84(%rsp)
	movw	$15, 88(%rsp)
	movw	$256, 90(%rsp)          # imm = 0x100
	movw	$4, 92(%rsp)
	movl	$1, 94(%rsp)
	cmpl	$2, %r13d
	setg	%cl
	movl	$1, %ebx
	movl	%r15d, 98(%rsp)
	movw	$257, 102(%rsp)         # imm = 0x101
	movw	$4, 104(%rsp)
	movl	$1, 106(%rsp)
	testb	%al, %al
	movl	%ebp, 110(%rsp)
	cmovel	%r14d, %ebx
	movzbl	%cl, %eax
	movl	360(%rsp), %edx
	leal	(,%rdx,8), %ecx
	incl	%eax
	cmpl	$1, %r13d
	movw	$258, 114(%rsp)         # imm = 0x102
	movw	$3, 116(%rsp)
	movl	$1, 118(%rsp)
	movw	%cx, 122(%rsp)
	movw	$259, 126(%rsp)         # imm = 0x103
	movw	$3, 128(%rsp)
	movl	$1, 130(%rsp)
	movw	$1, 134(%rsp)
	movw	$262, 138(%rsp)         # imm = 0x106
	movw	$3, 140(%rsp)
	movl	$1, 142(%rsp)
	movw	%ax, 146(%rsp)
	movw	$273, 150(%rsp)         # imm = 0x111
	movw	$4, 152(%rsp)
	movl	%r13d, 154(%rsp)
	movl	$210, 158(%rsp)
	movw	$277, 162(%rsp)         # imm = 0x115
	movw	$3, 164(%rsp)
	movl	$1, 166(%rsp)
	movw	%r13w, 170(%rsp)
	movw	$278, 174(%rsp)         # imm = 0x116
	movw	$4, 176(%rsp)
	movl	$1, 178(%rsp)
	movl	%ebp, 182(%rsp)
	jne	.LBB8_10
# BB#9:
	movslq	%edx, %rax
	imulq	40(%rsp), %rax          # 8-byte Folded Reload
	jmp	.LBB8_11
.LBB8_10:
	movslq	%r13d, %rax
	leaq	210(,%rax,4), %rax
.LBB8_11:
	movw	$279, 186(%rsp)         # imm = 0x117
	movw	$4, 188(%rsp)
	movl	%r13d, 190(%rsp)
	movl	%eax, 194(%rsp)
	movw	$282, 198(%rsp)         # imm = 0x11A
	movw	$5, 200(%rsp)
	movl	$1, 202(%rsp)
	movslq	352(%rsp), %rax
	leaq	_ZN12_GLOBAL__N_130pixel_type_to_tiff_sample_typeE(%rip), %rcx
	movw	(%rcx,%rax,2), %ax
	leaq	80(%rsp), %rdi
	movl	$194, 206(%rsp)
	movw	$283, 210(%rsp)         # imm = 0x11B
	movw	$5, 212(%rsp)
	movl	$1, 214(%rsp)
	movl	$202, 218(%rsp)
	movw	$284, 222(%rsp)         # imm = 0x11C
	movw	$3, 224(%rsp)
	movl	$1, 226(%rsp)
	movw	$2, 230(%rsp)
	movw	$296, 234(%rsp)         # imm = 0x128
	movw	$3, 236(%rsp)
	movl	$1, 238(%rsp)
	movw	$1, 242(%rsp)
	movw	$339, 246(%rsp)         # imm = 0x153
	movw	$3, 248(%rsp)
	movl	$1, 250(%rsp)
	movw	%ax, 254(%rsp)
	movw	$-32539, 258(%rsp)      # imm = 0xFFFFFFFFFFFF80E5
	movw	$4, 260(%rsp)
	movl	$1, 262(%rsp)
	movl	%ebx, 266(%rsp)
	movl	$0, 270(%rsp)
	movl	$1, 274(%rsp)
	movl	$1, 278(%rsp)
	movl	$1, 282(%rsp)
	movl	$1, 286(%rsp)
	movl	$210, %esi
	movl	$1, %edx
	movq	%r12, %rcx
	callq	fwrite@PLT
	movl	$-2, %r14d
	cmpl	$1, %eax
	jne	.LBB8_21
# BB#12:
	cmpl	$2, %r13d
	jl	.LBB8_20
# BB#13:                                # %.lr.ph.i
	imull	%r15d, %ebp
	movl	%ebp, 28(%rsp)          # 4-byte Spill
	leal	210(,%r13,8), %eax
	movl	%eax, 76(%rsp)
	imull	360(%rsp), %ebp
	imull	%ebx, %ebp
	movl	%ebx, 24(%rsp)          # 4-byte Spill
	xorl	%r15d, %r15d
	leaq	76(%rsp), %rbx
.LBB8_14:                               # =>This Inner Loop Header: Depth=1
	movq	%rbx, %rdi
	movl	$4, %esi
	movl	$1, %edx
	movq	%r12, %rcx
	callq	fwrite@PLT
	cmpl	$1, %eax
	jne	.LBB8_21
# BB#15:                                #   in Loop: Header=BB8_14 Depth=1
	addl	%ebp, 76(%rsp)
	incl	%r15d
	cmpl	%r13d, %r15d
	jl	.LBB8_14
# BB#16:                                # %._crit_edge.i
	movl	24(%rsp), %eax          # 4-byte Reload
	imull	28(%rsp), %eax          # 4-byte Folded Reload
	movl	%eax, 72(%rsp)
	movl	$-1, %ebp
	leaq	72(%rsp), %rbx
.LBB8_17:                               # =>This Inner Loop Header: Depth=1
	incl	%ebp
	cmpl	%r13d, %ebp
	jge	.LBB8_20
# BB#18:                                #   in Loop: Header=BB8_17 Depth=1
	movq	%rbx, %rdi
	movl	$4, %esi
	movl	$1, %edx
	movq	%r12, %rcx
	callq	fwrite@PLT
	cmpl	$1, %eax
	je	.LBB8_17
	jmp	.LBB8_21
.LBB8_20:                               # %.loopexit.i
	movslq	360(%rsp), %rsi
	imulq	40(%rsp), %rsi          # 8-byte Folded Reload
	movq	32(%rsp), %rdi          # 8-byte Reload
	movl	$1, %edx
	movq	%r12, %rcx
	callq	fwrite@PLT
	movl	$-1, %ecx
	xorl	%r14d, %r14d
	cmpl	$1, %eax
	cmovnel	%ecx, %r14d
.LBB8_21:                               # %_ZN12_GLOBAL__N_124halide_write_debug_imageEPKcPhiiiiiiPFbPKvmPvES5_.exit
	movq	%r12, %rdi
	callq	fclose@PLT
	movl	%r14d, %eax
.LBB8_22:
	addq	$296, %rsp              # imm = 0x128
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	ret
.Ltmp38:
	.size	halide_debug_to_file, .Ltmp38-halide_debug_to_file
	.cfi_endproc

	.section	.text.sqrt_f32,"axG",@progbits,sqrt_f32,comdat
	.weak	sqrt_f32
	.align	16, 0x90
	.type	sqrt_f32,@function
sqrt_f32:                               # @sqrt_f32
	.cfi_startproc
# BB#0:
	jmp	sqrtf@PLT               # TAILCALL
.Ltmp39:
	.size	sqrt_f32, .Ltmp39-sqrt_f32
	.cfi_endproc

	.section	.text.sin_f32,"axG",@progbits,sin_f32,comdat
	.weak	sin_f32
	.align	16, 0x90
	.type	sin_f32,@function
sin_f32:                                # @sin_f32
	.cfi_startproc
# BB#0:
	jmp	sinf@PLT                # TAILCALL
.Ltmp40:
	.size	sin_f32, .Ltmp40-sin_f32
	.cfi_endproc

	.section	.text.asin_f32,"axG",@progbits,asin_f32,comdat
	.weak	asin_f32
	.align	16, 0x90
	.type	asin_f32,@function
asin_f32:                               # @asin_f32
	.cfi_startproc
# BB#0:
	jmp	asinf@PLT               # TAILCALL
.Ltmp41:
	.size	asin_f32, .Ltmp41-asin_f32
	.cfi_endproc

	.section	.text.cos_f32,"axG",@progbits,cos_f32,comdat
	.weak	cos_f32
	.align	16, 0x90
	.type	cos_f32,@function
cos_f32:                                # @cos_f32
	.cfi_startproc
# BB#0:
	jmp	cosf@PLT                # TAILCALL
.Ltmp42:
	.size	cos_f32, .Ltmp42-cos_f32
	.cfi_endproc

	.section	.text.acos_f32,"axG",@progbits,acos_f32,comdat
	.weak	acos_f32
	.align	16, 0x90
	.type	acos_f32,@function
acos_f32:                               # @acos_f32
	.cfi_startproc
# BB#0:
	jmp	acosf@PLT               # TAILCALL
.Ltmp43:
	.size	acos_f32, .Ltmp43-acos_f32
	.cfi_endproc

	.section	.text.tan_f32,"axG",@progbits,tan_f32,comdat
	.weak	tan_f32
	.align	16, 0x90
	.type	tan_f32,@function
tan_f32:                                # @tan_f32
	.cfi_startproc
# BB#0:
	jmp	tanf@PLT                # TAILCALL
.Ltmp44:
	.size	tan_f32, .Ltmp44-tan_f32
	.cfi_endproc

	.section	.text.atan_f32,"axG",@progbits,atan_f32,comdat
	.weak	atan_f32
	.align	16, 0x90
	.type	atan_f32,@function
atan_f32:                               # @atan_f32
	.cfi_startproc
# BB#0:
	jmp	atanf@PLT               # TAILCALL
.Ltmp45:
	.size	atan_f32, .Ltmp45-atan_f32
	.cfi_endproc

	.section	.text.sinh_f32,"axG",@progbits,sinh_f32,comdat
	.weak	sinh_f32
	.align	16, 0x90
	.type	sinh_f32,@function
sinh_f32:                               # @sinh_f32
	.cfi_startproc
# BB#0:
	jmp	sinhf@PLT               # TAILCALL
.Ltmp46:
	.size	sinh_f32, .Ltmp46-sinh_f32
	.cfi_endproc

	.section	.text.asinh_f32,"axG",@progbits,asinh_f32,comdat
	.weak	asinh_f32
	.align	16, 0x90
	.type	asinh_f32,@function
asinh_f32:                              # @asinh_f32
	.cfi_startproc
# BB#0:
	jmp	asinhf@PLT              # TAILCALL
.Ltmp47:
	.size	asinh_f32, .Ltmp47-asinh_f32
	.cfi_endproc

	.section	.text.cosh_f32,"axG",@progbits,cosh_f32,comdat
	.weak	cosh_f32
	.align	16, 0x90
	.type	cosh_f32,@function
cosh_f32:                               # @cosh_f32
	.cfi_startproc
# BB#0:
	jmp	coshf@PLT               # TAILCALL
.Ltmp48:
	.size	cosh_f32, .Ltmp48-cosh_f32
	.cfi_endproc

	.section	.text.acosh_f32,"axG",@progbits,acosh_f32,comdat
	.weak	acosh_f32
	.align	16, 0x90
	.type	acosh_f32,@function
acosh_f32:                              # @acosh_f32
	.cfi_startproc
# BB#0:
	jmp	acoshf@PLT              # TAILCALL
.Ltmp49:
	.size	acosh_f32, .Ltmp49-acosh_f32
	.cfi_endproc

	.section	.text.tanh_f32,"axG",@progbits,tanh_f32,comdat
	.weak	tanh_f32
	.align	16, 0x90
	.type	tanh_f32,@function
tanh_f32:                               # @tanh_f32
	.cfi_startproc
# BB#0:
	jmp	tanhf@PLT               # TAILCALL
.Ltmp50:
	.size	tanh_f32, .Ltmp50-tanh_f32
	.cfi_endproc

	.section	.text.atanh_f32,"axG",@progbits,atanh_f32,comdat
	.weak	atanh_f32
	.align	16, 0x90
	.type	atanh_f32,@function
atanh_f32:                              # @atanh_f32
	.cfi_startproc
# BB#0:
	jmp	atanhf@PLT              # TAILCALL
.Ltmp51:
	.size	atanh_f32, .Ltmp51-atanh_f32
	.cfi_endproc

	.section	.text.hypot_f32,"axG",@progbits,hypot_f32,comdat
	.weak	hypot_f32
	.align	16, 0x90
	.type	hypot_f32,@function
hypot_f32:                              # @hypot_f32
	.cfi_startproc
# BB#0:
	jmp	hypotf@PLT              # TAILCALL
.Ltmp52:
	.size	hypot_f32, .Ltmp52-hypot_f32
	.cfi_endproc

	.section	.text.exp_f32,"axG",@progbits,exp_f32,comdat
	.weak	exp_f32
	.align	16, 0x90
	.type	exp_f32,@function
exp_f32:                                # @exp_f32
	.cfi_startproc
# BB#0:
	jmp	expf@PLT                # TAILCALL
.Ltmp53:
	.size	exp_f32, .Ltmp53-exp_f32
	.cfi_endproc

	.section	.text.log_f32,"axG",@progbits,log_f32,comdat
	.weak	log_f32
	.align	16, 0x90
	.type	log_f32,@function
log_f32:                                # @log_f32
	.cfi_startproc
# BB#0:
	jmp	logf@PLT                # TAILCALL
.Ltmp54:
	.size	log_f32, .Ltmp54-log_f32
	.cfi_endproc

	.section	.text.pow_f32,"axG",@progbits,pow_f32,comdat
	.weak	pow_f32
	.align	16, 0x90
	.type	pow_f32,@function
pow_f32:                                # @pow_f32
	.cfi_startproc
# BB#0:
	jmp	powf@PLT                # TAILCALL
.Ltmp55:
	.size	pow_f32, .Ltmp55-pow_f32
	.cfi_endproc

	.section	.text.floor_f32,"axG",@progbits,floor_f32,comdat
	.weak	floor_f32
	.align	16, 0x90
	.type	floor_f32,@function
floor_f32:                              # @floor_f32
	.cfi_startproc
# BB#0:
	roundss	$1, %xmm0, %xmm0
	ret
.Ltmp56:
	.size	floor_f32, .Ltmp56-floor_f32
	.cfi_endproc

	.section	.text.ceil_f32,"axG",@progbits,ceil_f32,comdat
	.weak	ceil_f32
	.align	16, 0x90
	.type	ceil_f32,@function
ceil_f32:                               # @ceil_f32
	.cfi_startproc
# BB#0:
	roundss	$2, %xmm0, %xmm0
	ret
.Ltmp57:
	.size	ceil_f32, .Ltmp57-ceil_f32
	.cfi_endproc

	.section	.text.round_f32,"axG",@progbits,round_f32,comdat
	.weak	round_f32
	.align	16, 0x90
	.type	round_f32,@function
round_f32:                              # @round_f32
	.cfi_startproc
# BB#0:
	jmp	roundf@PLT              # TAILCALL
.Ltmp58:
	.size	round_f32, .Ltmp58-round_f32
	.cfi_endproc

	.section	.text.sqrt_f64,"axG",@progbits,sqrt_f64,comdat
	.weak	sqrt_f64
	.align	16, 0x90
	.type	sqrt_f64,@function
sqrt_f64:                               # @sqrt_f64
	.cfi_startproc
# BB#0:
	jmp	sqrt@PLT                # TAILCALL
.Ltmp59:
	.size	sqrt_f64, .Ltmp59-sqrt_f64
	.cfi_endproc

	.section	.text.sin_f64,"axG",@progbits,sin_f64,comdat
	.weak	sin_f64
	.align	16, 0x90
	.type	sin_f64,@function
sin_f64:                                # @sin_f64
	.cfi_startproc
# BB#0:
	jmp	sin@PLT                 # TAILCALL
.Ltmp60:
	.size	sin_f64, .Ltmp60-sin_f64
	.cfi_endproc

	.section	.text.asin_f64,"axG",@progbits,asin_f64,comdat
	.weak	asin_f64
	.align	16, 0x90
	.type	asin_f64,@function
asin_f64:                               # @asin_f64
	.cfi_startproc
# BB#0:
	jmp	asin@PLT                # TAILCALL
.Ltmp61:
	.size	asin_f64, .Ltmp61-asin_f64
	.cfi_endproc

	.section	.text.cos_f64,"axG",@progbits,cos_f64,comdat
	.weak	cos_f64
	.align	16, 0x90
	.type	cos_f64,@function
cos_f64:                                # @cos_f64
	.cfi_startproc
# BB#0:
	jmp	cos@PLT                 # TAILCALL
.Ltmp62:
	.size	cos_f64, .Ltmp62-cos_f64
	.cfi_endproc

	.section	.text.acos_f64,"axG",@progbits,acos_f64,comdat
	.weak	acos_f64
	.align	16, 0x90
	.type	acos_f64,@function
acos_f64:                               # @acos_f64
	.cfi_startproc
# BB#0:
	jmp	acos@PLT                # TAILCALL
.Ltmp63:
	.size	acos_f64, .Ltmp63-acos_f64
	.cfi_endproc

	.section	.text.tan_f64,"axG",@progbits,tan_f64,comdat
	.weak	tan_f64
	.align	16, 0x90
	.type	tan_f64,@function
tan_f64:                                # @tan_f64
	.cfi_startproc
# BB#0:
	jmp	tan@PLT                 # TAILCALL
.Ltmp64:
	.size	tan_f64, .Ltmp64-tan_f64
	.cfi_endproc

	.section	.text.atan_f64,"axG",@progbits,atan_f64,comdat
	.weak	atan_f64
	.align	16, 0x90
	.type	atan_f64,@function
atan_f64:                               # @atan_f64
	.cfi_startproc
# BB#0:
	jmp	atan@PLT                # TAILCALL
.Ltmp65:
	.size	atan_f64, .Ltmp65-atan_f64
	.cfi_endproc

	.section	.text.sinh_f64,"axG",@progbits,sinh_f64,comdat
	.weak	sinh_f64
	.align	16, 0x90
	.type	sinh_f64,@function
sinh_f64:                               # @sinh_f64
	.cfi_startproc
# BB#0:
	jmp	sinh@PLT                # TAILCALL
.Ltmp66:
	.size	sinh_f64, .Ltmp66-sinh_f64
	.cfi_endproc

	.section	.text.asinh_f64,"axG",@progbits,asinh_f64,comdat
	.weak	asinh_f64
	.align	16, 0x90
	.type	asinh_f64,@function
asinh_f64:                              # @asinh_f64
	.cfi_startproc
# BB#0:
	jmp	asinh@PLT               # TAILCALL
.Ltmp67:
	.size	asinh_f64, .Ltmp67-asinh_f64
	.cfi_endproc

	.section	.text.cosh_f64,"axG",@progbits,cosh_f64,comdat
	.weak	cosh_f64
	.align	16, 0x90
	.type	cosh_f64,@function
cosh_f64:                               # @cosh_f64
	.cfi_startproc
# BB#0:
	jmp	cosh@PLT                # TAILCALL
.Ltmp68:
	.size	cosh_f64, .Ltmp68-cosh_f64
	.cfi_endproc

	.section	.text.acosh_f64,"axG",@progbits,acosh_f64,comdat
	.weak	acosh_f64
	.align	16, 0x90
	.type	acosh_f64,@function
acosh_f64:                              # @acosh_f64
	.cfi_startproc
# BB#0:
	jmp	acosh@PLT               # TAILCALL
.Ltmp69:
	.size	acosh_f64, .Ltmp69-acosh_f64
	.cfi_endproc

	.section	.text.tanh_f64,"axG",@progbits,tanh_f64,comdat
	.weak	tanh_f64
	.align	16, 0x90
	.type	tanh_f64,@function
tanh_f64:                               # @tanh_f64
	.cfi_startproc
# BB#0:
	jmp	tanh@PLT                # TAILCALL
.Ltmp70:
	.size	tanh_f64, .Ltmp70-tanh_f64
	.cfi_endproc

	.section	.text.atanh_f64,"axG",@progbits,atanh_f64,comdat
	.weak	atanh_f64
	.align	16, 0x90
	.type	atanh_f64,@function
atanh_f64:                              # @atanh_f64
	.cfi_startproc
# BB#0:
	jmp	atanh@PLT               # TAILCALL
.Ltmp71:
	.size	atanh_f64, .Ltmp71-atanh_f64
	.cfi_endproc

	.section	.text.hypot_f64,"axG",@progbits,hypot_f64,comdat
	.weak	hypot_f64
	.align	16, 0x90
	.type	hypot_f64,@function
hypot_f64:                              # @hypot_f64
	.cfi_startproc
# BB#0:
	jmp	hypot@PLT               # TAILCALL
.Ltmp72:
	.size	hypot_f64, .Ltmp72-hypot_f64
	.cfi_endproc

	.section	.text.exp_f64,"axG",@progbits,exp_f64,comdat
	.weak	exp_f64
	.align	16, 0x90
	.type	exp_f64,@function
exp_f64:                                # @exp_f64
	.cfi_startproc
# BB#0:
	jmp	exp@PLT                 # TAILCALL
.Ltmp73:
	.size	exp_f64, .Ltmp73-exp_f64
	.cfi_endproc

	.section	.text.log_f64,"axG",@progbits,log_f64,comdat
	.weak	log_f64
	.align	16, 0x90
	.type	log_f64,@function
log_f64:                                # @log_f64
	.cfi_startproc
# BB#0:
	jmp	log@PLT                 # TAILCALL
.Ltmp74:
	.size	log_f64, .Ltmp74-log_f64
	.cfi_endproc

	.section	.text.pow_f64,"axG",@progbits,pow_f64,comdat
	.weak	pow_f64
	.align	16, 0x90
	.type	pow_f64,@function
pow_f64:                                # @pow_f64
	.cfi_startproc
# BB#0:
	jmp	pow@PLT                 # TAILCALL
.Ltmp75:
	.size	pow_f64, .Ltmp75-pow_f64
	.cfi_endproc

	.section	.text.floor_f64,"axG",@progbits,floor_f64,comdat
	.weak	floor_f64
	.align	16, 0x90
	.type	floor_f64,@function
floor_f64:                              # @floor_f64
	.cfi_startproc
# BB#0:
	roundsd	$1, %xmm0, %xmm0
	ret
.Ltmp76:
	.size	floor_f64, .Ltmp76-floor_f64
	.cfi_endproc

	.section	.text.ceil_f64,"axG",@progbits,ceil_f64,comdat
	.weak	ceil_f64
	.align	16, 0x90
	.type	ceil_f64,@function
ceil_f64:                               # @ceil_f64
	.cfi_startproc
# BB#0:
	roundsd	$2, %xmm0, %xmm0
	ret
.Ltmp77:
	.size	ceil_f64, .Ltmp77-ceil_f64
	.cfi_endproc

	.section	.text.round_f64,"axG",@progbits,round_f64,comdat
	.weak	round_f64
	.align	16, 0x90
	.type	round_f64,@function
round_f64:                              # @round_f64
	.cfi_startproc
# BB#0:
	jmp	round@PLT               # TAILCALL
.Ltmp78:
	.size	round_f64, .Ltmp78-round_f64
	.cfi_endproc

	.section	.rodata.cst4,"aM",@progbits,4
	.align	4
.LCPI49_0:
	.long	2139095039              # float 3.402823e+38
                                        #  (0x7f7fffff)
	.section	.text.maxval_f32,"axG",@progbits,maxval_f32,comdat
	.weak	maxval_f32
	.align	16, 0x90
	.type	maxval_f32,@function
maxval_f32:                             # @maxval_f32
	.cfi_startproc
# BB#0:
	movss	.LCPI49_0(%rip), %xmm0
	ret
.Ltmp79:
	.size	maxval_f32, .Ltmp79-maxval_f32
	.cfi_endproc

	.section	.rodata.cst4,"aM",@progbits,4
	.align	4
.LCPI50_0:
	.long	4286578687              # float -3.402823e+38
                                        #  (0xff7fffff)
	.section	.text.minval_f32,"axG",@progbits,minval_f32,comdat
	.weak	minval_f32
	.align	16, 0x90
	.type	minval_f32,@function
minval_f32:                             # @minval_f32
	.cfi_startproc
# BB#0:
	movss	.LCPI50_0(%rip), %xmm0
	ret
.Ltmp80:
	.size	minval_f32, .Ltmp80-minval_f32
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.align	8
.LCPI51_0:
	.quad	9218868437227405311     # double 1.797693e+308
                                        #  (0x7fefffffffffffff)
	.section	.text.maxval_f64,"axG",@progbits,maxval_f64,comdat
	.weak	maxval_f64
	.align	16, 0x90
	.type	maxval_f64,@function
maxval_f64:                             # @maxval_f64
	.cfi_startproc
# BB#0:
	movsd	.LCPI51_0(%rip), %xmm0
	ret
.Ltmp81:
	.size	maxval_f64, .Ltmp81-maxval_f64
	.cfi_endproc

	.section	.rodata.cst8,"aM",@progbits,8
	.align	8
.LCPI52_0:
	.quad	-4503599627370497       # double -1.797693e+308
                                        #  (0xffefffffffffffff)
	.section	.text.minval_f64,"axG",@progbits,minval_f64,comdat
	.weak	minval_f64
	.align	16, 0x90
	.type	minval_f64,@function
minval_f64:                             # @minval_f64
	.cfi_startproc
# BB#0:
	movsd	.LCPI52_0(%rip), %xmm0
	ret
.Ltmp82:
	.size	minval_f64, .Ltmp82-minval_f64
	.cfi_endproc

	.section	.text.maxval_u8,"axG",@progbits,maxval_u8,comdat
	.weak	maxval_u8
	.align	16, 0x90
	.type	maxval_u8,@function
maxval_u8:                              # @maxval_u8
	.cfi_startproc
# BB#0:
	movl	$255, %eax
	ret
.Ltmp83:
	.size	maxval_u8, .Ltmp83-maxval_u8
	.cfi_endproc

	.section	.text.minval_u8,"axG",@progbits,minval_u8,comdat
	.weak	minval_u8
	.align	16, 0x90
	.type	minval_u8,@function
minval_u8:                              # @minval_u8
	.cfi_startproc
# BB#0:
	xorl	%eax, %eax
	ret
.Ltmp84:
	.size	minval_u8, .Ltmp84-minval_u8
	.cfi_endproc

	.section	.text.maxval_u16,"axG",@progbits,maxval_u16,comdat
	.weak	maxval_u16
	.align	16, 0x90
	.type	maxval_u16,@function
maxval_u16:                             # @maxval_u16
	.cfi_startproc
# BB#0:
	movl	$65535, %eax            # imm = 0xFFFF
	ret
.Ltmp85:
	.size	maxval_u16, .Ltmp85-maxval_u16
	.cfi_endproc

	.section	.text.minval_u16,"axG",@progbits,minval_u16,comdat
	.weak	minval_u16
	.align	16, 0x90
	.type	minval_u16,@function
minval_u16:                             # @minval_u16
	.cfi_startproc
# BB#0:
	xorl	%eax, %eax
	ret
.Ltmp86:
	.size	minval_u16, .Ltmp86-minval_u16
	.cfi_endproc

	.section	.text.maxval_u32,"axG",@progbits,maxval_u32,comdat
	.weak	maxval_u32
	.align	16, 0x90
	.type	maxval_u32,@function
maxval_u32:                             # @maxval_u32
	.cfi_startproc
# BB#0:
	movl	$-1, %eax
	ret
.Ltmp87:
	.size	maxval_u32, .Ltmp87-maxval_u32
	.cfi_endproc

	.section	.text.minval_u32,"axG",@progbits,minval_u32,comdat
	.weak	minval_u32
	.align	16, 0x90
	.type	minval_u32,@function
minval_u32:                             # @minval_u32
	.cfi_startproc
# BB#0:
	xorl	%eax, %eax
	ret
.Ltmp88:
	.size	minval_u32, .Ltmp88-minval_u32
	.cfi_endproc

	.section	.text.maxval_u64,"axG",@progbits,maxval_u64,comdat
	.weak	maxval_u64
	.align	16, 0x90
	.type	maxval_u64,@function
maxval_u64:                             # @maxval_u64
	.cfi_startproc
# BB#0:
	movq	$-1, %rax
	ret
.Ltmp89:
	.size	maxval_u64, .Ltmp89-maxval_u64
	.cfi_endproc

	.section	.text.minval_u64,"axG",@progbits,minval_u64,comdat
	.weak	minval_u64
	.align	16, 0x90
	.type	minval_u64,@function
minval_u64:                             # @minval_u64
	.cfi_startproc
# BB#0:
	xorl	%eax, %eax
	ret
.Ltmp90:
	.size	minval_u64, .Ltmp90-minval_u64
	.cfi_endproc

	.section	.text.maxval_s8,"axG",@progbits,maxval_s8,comdat
	.weak	maxval_s8
	.align	16, 0x90
	.type	maxval_s8,@function
maxval_s8:                              # @maxval_s8
	.cfi_startproc
# BB#0:
	movl	$127, %eax
	ret
.Ltmp91:
	.size	maxval_s8, .Ltmp91-maxval_s8
	.cfi_endproc

	.section	.text.minval_s8,"axG",@progbits,minval_s8,comdat
	.weak	minval_s8
	.align	16, 0x90
	.type	minval_s8,@function
minval_s8:                              # @minval_s8
	.cfi_startproc
# BB#0:
	movl	$-128, %eax
	ret
.Ltmp92:
	.size	minval_s8, .Ltmp92-minval_s8
	.cfi_endproc

	.section	.text.maxval_s16,"axG",@progbits,maxval_s16,comdat
	.weak	maxval_s16
	.align	16, 0x90
	.type	maxval_s16,@function
maxval_s16:                             # @maxval_s16
	.cfi_startproc
# BB#0:
	movl	$32767, %eax            # imm = 0x7FFF
	ret
.Ltmp93:
	.size	maxval_s16, .Ltmp93-maxval_s16
	.cfi_endproc

	.section	.text.minval_s16,"axG",@progbits,minval_s16,comdat
	.weak	minval_s16
	.align	16, 0x90
	.type	minval_s16,@function
minval_s16:                             # @minval_s16
	.cfi_startproc
# BB#0:
	movl	$-32768, %eax           # imm = 0xFFFFFFFFFFFF8000
	ret
.Ltmp94:
	.size	minval_s16, .Ltmp94-minval_s16
	.cfi_endproc

	.section	.text.maxval_s32,"axG",@progbits,maxval_s32,comdat
	.weak	maxval_s32
	.align	16, 0x90
	.type	maxval_s32,@function
maxval_s32:                             # @maxval_s32
	.cfi_startproc
# BB#0:
	movl	$2147483647, %eax       # imm = 0x7FFFFFFF
	ret
.Ltmp95:
	.size	maxval_s32, .Ltmp95-maxval_s32
	.cfi_endproc

	.section	.text.minval_s32,"axG",@progbits,minval_s32,comdat
	.weak	minval_s32
	.align	16, 0x90
	.type	minval_s32,@function
minval_s32:                             # @minval_s32
	.cfi_startproc
# BB#0:
	movl	$-2147483648, %eax      # imm = 0xFFFFFFFF80000000
	ret
.Ltmp96:
	.size	minval_s32, .Ltmp96-minval_s32
	.cfi_endproc

	.section	.text.maxval_s64,"axG",@progbits,maxval_s64,comdat
	.weak	maxval_s64
	.align	16, 0x90
	.type	maxval_s64,@function
maxval_s64:                             # @maxval_s64
	.cfi_startproc
# BB#0:
	movabsq	$9223372036854775807, %rax # imm = 0x7FFFFFFFFFFFFFFF
	ret
.Ltmp97:
	.size	maxval_s64, .Ltmp97-maxval_s64
	.cfi_endproc

	.section	.text.minval_s64,"axG",@progbits,minval_s64,comdat
	.weak	minval_s64
	.align	16, 0x90
	.type	minval_s64,@function
minval_s64:                             # @minval_s64
	.cfi_startproc
# BB#0:
	movabsq	$-9223372036854775808, %rax # imm = 0x8000000000000000
	ret
.Ltmp98:
	.size	minval_s64, .Ltmp98-minval_s64
	.cfi_endproc

	.section	.text.abs_i8,"axG",@progbits,abs_i8,comdat
	.weak	abs_i8
	.align	16, 0x90
	.type	abs_i8,@function
abs_i8:                                 # @abs_i8
	.cfi_startproc
# BB#0:
	movb	%dil, %al
	sarb	$7, %al
	addb	%al, %dil
	xorb	%al, %dil
	movsbl	%dil, %eax
	ret
.Ltmp99:
	.size	abs_i8, .Ltmp99-abs_i8
	.cfi_endproc

	.section	.text.abs_i16,"axG",@progbits,abs_i16,comdat
	.weak	abs_i16
	.align	16, 0x90
	.type	abs_i16,@function
abs_i16:                                # @abs_i16
	.cfi_startproc
# BB#0:
	movw	%di, %ax
	negw	%ax
	cmovlw	%di, %ax
	movswl	%ax, %eax
	ret
.Ltmp100:
	.size	abs_i16, .Ltmp100-abs_i16
	.cfi_endproc

	.section	.text.abs_i32,"axG",@progbits,abs_i32,comdat
	.weak	abs_i32
	.align	16, 0x90
	.type	abs_i32,@function
abs_i32:                                # @abs_i32
	.cfi_startproc
# BB#0:
	movl	%edi, %eax
	negl	%eax
	cmovll	%edi, %eax
	ret
.Ltmp101:
	.size	abs_i32, .Ltmp101-abs_i32
	.cfi_endproc

	.section	.text.abs_i64,"axG",@progbits,abs_i64,comdat
	.weak	abs_i64
	.align	16, 0x90
	.type	abs_i64,@function
abs_i64:                                # @abs_i64
	.cfi_startproc
# BB#0:
	movq	%rdi, %rax
	negq	%rax
	cmovlq	%rdi, %rax
	ret
.Ltmp102:
	.size	abs_i64, .Ltmp102-abs_i64
	.cfi_endproc

	.section	.rodata.cst16,"aM",@progbits,16
	.align	16
.LCPI73_0:
	.long	2147483648              # float -0.000000e+00
	.long	2147483648              # float -0.000000e+00
	.long	2147483648              # float -0.000000e+00
	.long	2147483648              # float -0.000000e+00
	.section	.text.abs_f32,"axG",@progbits,abs_f32,comdat
	.weak	abs_f32
	.align	16, 0x90
	.type	abs_f32,@function
abs_f32:                                # @abs_f32
	.cfi_startproc
# BB#0:
	xorps	%xmm1, %xmm1
	ucomiss	%xmm1, %xmm0
	jae	.LBB73_2
# BB#1:
	xorps	.LCPI73_0(%rip), %xmm0
.LBB73_2:
	ret
.Ltmp103:
	.size	abs_f32, .Ltmp103-abs_f32
	.cfi_endproc

	.section	.rodata.cst16,"aM",@progbits,16
	.align	16
.LCPI74_0:
	.quad	-9223372036854775808    # double -0.000000e+00
	.quad	-9223372036854775808    # double -0.000000e+00
	.section	.text.abs_f64,"axG",@progbits,abs_f64,comdat
	.weak	abs_f64
	.align	16, 0x90
	.type	abs_f64,@function
abs_f64:                                # @abs_f64
	.cfi_startproc
# BB#0:
	xorpd	%xmm1, %xmm1
	ucomisd	%xmm1, %xmm0
	jae	.LBB74_2
# BB#1:
	xorpd	.LCPI74_0(%rip), %xmm0
.LBB74_2:
	ret
.Ltmp104:
	.size	abs_f64, .Ltmp104-abs_f64
	.cfi_endproc

	.section	.text.halide_shutdown_thread_pool,"axG",@progbits,halide_shutdown_thread_pool,comdat
	.weak	halide_shutdown_thread_pool
	.align	16, 0x90
	.type	halide_shutdown_thread_pool,@function
halide_shutdown_thread_pool:            # @halide_shutdown_thread_pool
	.cfi_startproc
# BB#0:
	pushq	%r15
.Ltmp111:
	.cfi_def_cfa_offset 16
	pushq	%r14
.Ltmp112:
	.cfi_def_cfa_offset 24
	pushq	%r13
.Ltmp113:
	.cfi_def_cfa_offset 32
	pushq	%r12
.Ltmp114:
	.cfi_def_cfa_offset 40
	pushq	%rbx
.Ltmp115:
	.cfi_def_cfa_offset 48
	subq	$16, %rsp
.Ltmp116:
	.cfi_def_cfa_offset 64
.Ltmp117:
	.cfi_offset %rbx, -48
.Ltmp118:
	.cfi_offset %r12, -40
.Ltmp119:
	.cfi_offset %r13, -32
.Ltmp120:
	.cfi_offset %r14, -24
.Ltmp121:
	.cfi_offset %r15, -16
	movq	halide_thread_pool_initialized@GOTPCREL(%rip), %r14
	cmpb	$0, (%r14)
	je	.LBB75_5
# BB#1:
	leaq	halide_work_queue+2097160(%rip), %rbx
	movq	%rbx, %rdi
	callq	pthread_mutex_lock@PLT
	movb	$1, halide_work_queue+2097764(%rip)
	leaq	halide_work_queue+2097200(%rip), %rdi
	callq	pthread_cond_broadcast@PLT
	movq	%rbx, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	halide_threads@GOTPCREL(%rip), %r15
	movl	(%r15), %eax
	decl	%eax
	testl	%eax, %eax
	jle	.LBB75_4
# BB#2:
	xorl	%ebx, %ebx
	leaq	halide_work_queue(%rip), %r13
	leaq	8(%rsp), %r12
	.align	16, 0x90
.LBB75_3:                               # %.lr.ph
                                        # =>This Inner Loop Header: Depth=1
	movq	2097248(%r13,%rbx,8), %rdi
	movq	%r12, %rsi
	callq	pthread_join@PLT
	incq	%rbx
	movl	(%r15), %eax
	decl	%eax
	cmpl	%eax, %ebx
	jl	.LBB75_3
.LBB75_4:                               # %._crit_edge
	leaq	halide_work_queue+2097160(%rip), %rdi
	callq	pthread_mutex_destroy@PLT
	leaq	halide_work_queue+2097200(%rip), %rdi
	callq	pthread_cond_destroy@PLT
	movb	$0, (%r14)
.LBB75_5:
	addq	$16, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	ret
.Ltmp122:
	.size	halide_shutdown_thread_pool, .Ltmp122-halide_shutdown_thread_pool
	.cfi_endproc

	.section	.text.set_halide_custom_do_task,"axG",@progbits,set_halide_custom_do_task,comdat
	.weak	set_halide_custom_do_task
	.align	16, 0x90
	.type	set_halide_custom_do_task,@function
set_halide_custom_do_task:              # @set_halide_custom_do_task
	.cfi_startproc
# BB#0:
	movq	halide_custom_do_task@GOTPCREL(%rip), %rax
	movq	%rdi, (%rax)
	ret
.Ltmp123:
	.size	set_halide_custom_do_task, .Ltmp123-set_halide_custom_do_task
	.cfi_endproc

	.section	.text.set_halide_custom_do_par_for,"axG",@progbits,set_halide_custom_do_par_for,comdat
	.weak	set_halide_custom_do_par_for
	.align	16, 0x90
	.type	set_halide_custom_do_par_for,@function
set_halide_custom_do_par_for:           # @set_halide_custom_do_par_for
	.cfi_startproc
# BB#0:
	movq	halide_custom_do_par_for@GOTPCREL(%rip), %rax
	movq	%rdi, (%rax)
	ret
.Ltmp124:
	.size	set_halide_custom_do_par_for, .Ltmp124-set_halide_custom_do_par_for
	.cfi_endproc

	.section	.text.halide_do_task,"axG",@progbits,halide_do_task,comdat
	.weak	halide_do_task
	.align	16, 0x90
	.type	halide_do_task,@function
halide_do_task:                         # @halide_do_task
	.cfi_startproc
# BB#0:
	movq	%rdi, %rax
	movq	halide_custom_do_task@GOTPCREL(%rip), %rcx
	movq	(%rcx), %rcx
	testq	%rcx, %rcx
	je	.LBB78_2
# BB#1:
	movq	%rax, %rdi
	jmpq	*%rcx  # TAILCALL
.LBB78_2:
	movl	%esi, %edi
	movq	%rdx, %rsi
	jmpq	*%rax  # TAILCALL
.Ltmp125:
	.size	halide_do_task, .Ltmp125-halide_do_task
	.cfi_endproc

	.section	.text.halide_worker_thread,"axG",@progbits,halide_worker_thread,comdat
	.weak	halide_worker_thread
	.align	16, 0x90
	.type	halide_worker_thread,@function
halide_worker_thread:                   # @halide_worker_thread
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Ltmp133:
	.cfi_def_cfa_offset 16
	pushq	%r15
.Ltmp134:
	.cfi_def_cfa_offset 24
	pushq	%r14
.Ltmp135:
	.cfi_def_cfa_offset 32
	pushq	%r13
.Ltmp136:
	.cfi_def_cfa_offset 40
	pushq	%r12
.Ltmp137:
	.cfi_def_cfa_offset 48
	pushq	%rbx
.Ltmp138:
	.cfi_def_cfa_offset 56
	pushq	%rax
.Ltmp139:
	.cfi_def_cfa_offset 64
.Ltmp140:
	.cfi_offset %rbx, -56
.Ltmp141:
	.cfi_offset %r12, -48
.Ltmp142:
	.cfi_offset %r13, -40
.Ltmp143:
	.cfi_offset %r14, -32
.Ltmp144:
	.cfi_offset %r15, -24
.Ltmp145:
	.cfi_offset %rbp, -16
	movq	%rdi, %rbx
	leaq	halide_work_queue+2097160(%rip), %rdi
	callq	pthread_mutex_lock@PLT
	testq	%rbx, %rbx
	je	.LBB79_12
# BB#1:                                 # %.backedge.preheader
	movq	8(%rbx), %rax
	movl	24(%rax), %ecx
	cmpl	(%rbx), %ecx
	movq	%rbx, (%rsp)            # 8-byte Spill
	jne	.LBB79_7
# BB#2:
	leaq	halide_work_queue+2097160(%rip), %rbx
	leaq	halide_work_queue(%rip), %r14
	.align	16, 0x90
.LBB79_3:                               # %.lr.ph4
                                        # =>This Inner Loop Header: Depth=1
	cmpb	$0, halide_work_queue+2097764(%rip)
	jne	.LBB79_11
# BB#4:                                 #   in Loop: Header=BB79_3 Depth=1
	movq	halide_work_queue+2097152(%rip), %rax
	movq	%rax, %rcx
	shrq	$32, %rcx
	cmpl	%ecx, %eax
	jne	.LBB79_20
# BB#5:                                 #   in Loop: Header=BB79_3 Depth=1
	leaq	halide_work_queue+2097200(%rip), %rdi
	movq	%rbx, %rsi
	callq	pthread_cond_wait@PLT
	jmp	.LBB79_6
	.align	16, 0x90
.LBB79_20:                              #   in Loop: Header=BB79_3 Depth=1
	movq	%rbx, %rsi
	movslq	%eax, %r15
	shlq	$5, %r15
	movl	8(%r15,%r14), %ebp
	cmpl	12(%r15,%r14), %ebp
	jne	.LBB79_22
# BB#21:                                #   in Loop: Header=BB79_3 Depth=1
	leal	1(%rax), %ecx
	movl	%ecx, %edx
	sarl	$31, %edx
	shrl	$16, %edx
	leal	1(%rax,%rdx), %eax
	andl	$-65536, %eax           # imm = 0xFFFFFFFFFFFF0000
	subl	%eax, %ecx
	movl	%ecx, halide_work_queue+2097152(%rip)
	movl	$0, 24(%r15,%r14)
	movq	%rsi, %rbx
	jmp	.LBB79_6
	.align	16, 0x90
.LBB79_22:                              #   in Loop: Header=BB79_3 Depth=1
	leaq	8(%r15,%r14), %rax
	movq	(%r15,%r14), %rbx
	movq	16(%r15,%r14), %r12
	leal	1(%rbp), %ecx
	movl	%ecx, (%rax)
	incl	28(%r15,%r14)
	movq	%r14, %r13
	movq	%rsi, %r14
	movq	%r14, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%rbx, %rdi
	movq	%r14, %rbx
	movq	%r13, %r14
	movl	%ebp, %esi
	movq	%r12, %rdx
	callq	halide_do_task@PLT
	movq	%rbx, %rdi
	callq	pthread_mutex_lock@PLT
	decl	28(%r15,%r14)
.LBB79_6:                               #   in Loop: Header=BB79_3 Depth=1
	movq	%rbx, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%rbx, %rdi
	callq	pthread_mutex_lock@PLT
	movq	(%rsp), %rdx            # 8-byte Reload
	movq	8(%rdx), %rax
	movl	24(%rax), %ecx
	cmpl	(%rdx), %ecx
	je	.LBB79_3
.LBB79_7:                               # %.us-lcssa
	cmpl	$0, 28(%rax)
	movq	(%rsp), %r12            # 8-byte Reload
	je	.LBB79_11
# BB#8:
	leaq	halide_work_queue+2097160(%rip), %rbx
	movq	%rbx, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%rbx, %rdi
	callq	pthread_mutex_lock@PLT
	movq	8(%r12), %rax
	cmpl	$0, 28(%rax)
	je	.LBB79_11
# BB#9:
	leaq	halide_work_queue+2097160(%rip), %rbx
	.align	16, 0x90
.LBB79_10:                              # %.lr.ph
                                        # =>This Inner Loop Header: Depth=1
	movq	%rbx, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%rbx, %rdi
	callq	pthread_mutex_lock@PLT
	movq	8(%r12), %rax
	cmpl	$0, 28(%rax)
	jne	.LBB79_10
	jmp	.LBB79_11
.LBB79_12:                              # %.backedge.us.preheader
	cmpb	$0, halide_work_queue+2097764(%rip)
	jne	.LBB79_11
# BB#13:
	leaq	halide_work_queue+2097160(%rip), %r15
	leaq	halide_work_queue(%rip), %r13
	.align	16, 0x90
.LBB79_14:                              # %.lr.ph1
                                        # =>This Inner Loop Header: Depth=1
	movq	halide_work_queue+2097152(%rip), %rax
	movq	%rax, %rcx
	shrq	$32, %rcx
	cmpl	%ecx, %eax
	jne	.LBB79_15
# BB#19:                                #   in Loop: Header=BB79_14 Depth=1
	leaq	halide_work_queue+2097200(%rip), %rdi
	movq	%r15, %rsi
	callq	pthread_cond_wait@PLT
	jmp	.LBB79_17
	.align	16, 0x90
.LBB79_15:                              #   in Loop: Header=BB79_14 Depth=1
	movslq	%eax, %r14
	shlq	$5, %r14
	movl	8(%r14,%r13), %ebp
	cmpl	12(%r14,%r13), %ebp
	jne	.LBB79_16
# BB#18:                                #   in Loop: Header=BB79_14 Depth=1
	leal	1(%rax), %ecx
	movl	%ecx, %edx
	sarl	$31, %edx
	shrl	$16, %edx
	leal	1(%rax,%rdx), %eax
	andl	$-65536, %eax           # imm = 0xFFFFFFFFFFFF0000
	subl	%eax, %ecx
	movl	%ecx, halide_work_queue+2097152(%rip)
	movl	$0, 24(%r14,%r13)
	jmp	.LBB79_17
	.align	16, 0x90
.LBB79_16:                              #   in Loop: Header=BB79_14 Depth=1
	leaq	8(%r14,%r13), %rax
	movq	(%r14,%r13), %rbx
	movq	16(%r14,%r13), %r12
	leal	1(%rbp), %ecx
	movl	%ecx, (%rax)
	incl	28(%r14,%r13)
	movq	%r15, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%rbx, %rdi
	movl	%ebp, %esi
	movq	%r12, %rdx
	callq	halide_do_task@PLT
	movq	%r15, %rdi
	callq	pthread_mutex_lock@PLT
	decl	28(%r14,%r13)
.LBB79_17:                              #   in Loop: Header=BB79_14 Depth=1
	movq	%r15, %rdi
	callq	pthread_mutex_unlock@PLT
	movq	%r15, %rdi
	callq	pthread_mutex_lock@PLT
	cmpb	$0, halide_work_queue+2097764(%rip)
	je	.LBB79_14
.LBB79_11:                              # %.loopexit
	leaq	halide_work_queue+2097160(%rip), %rdi
	callq	pthread_mutex_unlock@PLT
	xorl	%eax, %eax
	addq	$8, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	ret
.Ltmp146:
	.size	halide_worker_thread, .Ltmp146-halide_worker_thread
	.cfi_endproc

	.section	.text.halide_do_par_for,"axG",@progbits,halide_do_par_for,comdat
	.weak	halide_do_par_for
	.align	16, 0x90
	.type	halide_do_par_for,@function
halide_do_par_for:                      # @halide_do_par_for
	.cfi_startproc
# BB#0:
	pushq	%rbp
.Ltmp154:
	.cfi_def_cfa_offset 16
	pushq	%r15
.Ltmp155:
	.cfi_def_cfa_offset 24
	pushq	%r14
.Ltmp156:
	.cfi_def_cfa_offset 32
	pushq	%r13
.Ltmp157:
	.cfi_def_cfa_offset 40
	pushq	%r12
.Ltmp158:
	.cfi_def_cfa_offset 48
	pushq	%rbx
.Ltmp159:
	.cfi_def_cfa_offset 56
	subq	$40, %rsp
.Ltmp160:
	.cfi_def_cfa_offset 96
.Ltmp161:
	.cfi_offset %rbx, -56
.Ltmp162:
	.cfi_offset %r12, -48
.Ltmp163:
	.cfi_offset %r13, -40
.Ltmp164:
	.cfi_offset %r14, -32
.Ltmp165:
	.cfi_offset %r15, -24
.Ltmp166:
	.cfi_offset %rbp, -16
	movl	%edx, %ebp
	movl	%esi, %r13d
	movq	%rdi, %r15
	movq	halide_custom_do_par_for@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	testq	%rax, %rax
	je	.LBB80_2
# BB#1:
	movq	%r15, %rdi
	movl	%r13d, %esi
	movl	%ebp, %edx
	callq	*%rax
	jmp	.LBB80_13
.LBB80_2:
	movq	%rcx, 16(%rsp)          # 8-byte Spill
	movq	halide_thread_pool_initialized@GOTPCREL(%rip), %rax
	cmpb	$0, (%rax)
	jne	.LBB80_12
# BB#3:
	movb	$0, halide_work_queue+2097764(%rip)
	leaq	halide_work_queue+2097160(%rip), %rdi
	xorl	%esi, %esi
	callq	pthread_mutex_init@PLT
	leaq	halide_work_queue+2097200(%rip), %rdi
	xorl	%esi, %esi
	callq	pthread_cond_init@PLT
	movl	$0, halide_work_queue+2097156(%rip)
	movl	$0, halide_work_queue+2097152(%rip)
	leaq	.L.str2(%rip), %rdi
	movl	$1, halide_work_queue+2097760(%rip)
	callq	getenv@PLT
	movq	halide_threads@GOTPCREL(%rip), %r12
	movl	$8, (%r12)
	testq	%rax, %rax
	je	.LBB80_5
# BB#4:
	movq	%rax, %rdi
	callq	atoi@PLT
	movl	%eax, (%r12)
	jmp	.LBB80_6
.LBB80_5:
	leaq	.L.str3(%rip), %rdi
	xorb	%al, %al
	callq	halide_printf@PLT
	movl	(%r12), %eax
.LBB80_6:
	cmpl	$65, %eax
	jl	.LBB80_8
# BB#7:                                 # %.preheader.thread
	movl	$64, (%r12)
	jmp	.LBB80_9
.LBB80_8:                               # %.preheader
	decl	%eax
	testl	%eax, %eax
	jle	.LBB80_11
.LBB80_9:
	xorl	%r14d, %r14d
	leaq	halide_work_queue+2097248(%rip), %rbx
	.align	16, 0x90
.LBB80_10:                              # %.lr.ph
                                        # =>This Inner Loop Header: Depth=1
	movq	halide_worker_thread@GOTPCREL(%rip), %rdx
	movq	%rbx, %rdi
	xorl	%esi, %esi
	xorl	%ecx, %ecx
	callq	pthread_create@PLT
	addq	$8, %rbx
	incl	%r14d
	movl	(%r12), %eax
	decl	%eax
	cmpl	%eax, %r14d
	jl	.LBB80_10
.LBB80_11:                              # %._crit_edge
	movq	halide_thread_pool_initialized@GOTPCREL(%rip), %rax
	movb	$1, (%rax)
.LBB80_12:
	leaq	halide_work_queue+2097160(%rip), %r12
	movq	%r12, %rdi
	callq	pthread_mutex_lock@PLT
	movl	halide_work_queue+2097760(%rip), %eax
	leal	2(%rax), %edx
	leal	1(%rax), %ecx
	testl	%eax, %eax
	cmovnel	%ecx, %edx
	movl	%edx, halide_work_queue+2097760(%rip)
	cmovel	%ecx, %eax
	addl	%r13d, %ebp
	leaq	halide_work_queue(%rip), %rcx
	movslq	halide_work_queue+2097156(%rip), %rdx
	shlq	$5, %rdx
	movq	%r15, (%rdx,%rcx)
	movl	%r13d, 8(%rdx,%rcx)
	movl	%ebp, 12(%rdx,%rcx)
	movq	16(%rsp), %rsi          # 8-byte Reload
	movq	%rsi, 16(%rdx,%rcx)
	movl	%eax, 24(%rdx,%rcx)
	movl	$0, 28(%rdx,%rcx)
	movslq	halide_work_queue+2097156(%rip), %rdx
	movl	%eax, 24(%rsp)
	movq	%rdx, %rax
	shlq	$5, %rax
	addq	%rcx, %rax
	movq	%rax, 32(%rsp)
	leal	1(%rdx), %eax
	movl	%eax, %ecx
	sarl	$31, %ecx
	shrl	$16, %ecx
	leal	1(%rdx,%rcx), %ecx
	andl	$-65536, %ecx           # imm = 0xFFFFFFFFFFFF0000
	subl	%ecx, %eax
	movl	%eax, halide_work_queue+2097156(%rip)
	movq	%r12, %rdi
	callq	pthread_mutex_unlock@PLT
	leaq	halide_work_queue+2097200(%rip), %rdi
	callq	pthread_cond_broadcast@PLT
	leaq	24(%rsp), %rdi
	callq	halide_worker_thread@PLT
.LBB80_13:
	addq	$40, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	ret
.Ltmp167:
	.size	halide_do_par_for, .Ltmp167-halide_do_par_for
	.cfi_endproc

	.section	.text.halide_copy_to_host,"axG",@progbits,halide_copy_to_host,comdat
	.weak	halide_copy_to_host
	.align	16, 0x90
	.type	halide_copy_to_host,@function
halide_copy_to_host:                    # @halide_copy_to_host
	.cfi_startproc
# BB#0:
	ret
.Ltmp168:
	.size	halide_copy_to_host, .Ltmp168-halide_copy_to_host
	.cfi_endproc

	.section	.text.packsswbx16,"axG",@progbits,packsswbx16,comdat
	.weak	packsswbx16
	.align	16, 0x90
	.type	packsswbx16,@function
packsswbx16:                            # @packsswbx16
# BB#0:
	packsswb	%xmm1, %xmm0
	ret
.Ltmp169:
	.size	packsswbx16, .Ltmp169-packsswbx16

	.section	.text.packuswbx16,"axG",@progbits,packuswbx16,comdat
	.weak	packuswbx16
	.align	16, 0x90
	.type	packuswbx16,@function
packuswbx16:                            # @packuswbx16
# BB#0:
	packuswb	%xmm1, %xmm0
	ret
.Ltmp170:
	.size	packuswbx16, .Ltmp170-packuswbx16

	.section	.text.packssdwx8,"axG",@progbits,packssdwx8,comdat
	.weak	packssdwx8
	.align	16, 0x90
	.type	packssdwx8,@function
packssdwx8:                             # @packssdwx8
# BB#0:
	packssdw	%xmm1, %xmm0
	ret
.Ltmp171:
	.size	packssdwx8, .Ltmp171-packssdwx8

	.section	.text.packusdwx8,"axG",@progbits,packusdwx8,comdat
	.weak	packusdwx8
	.align	16, 0x90
	.type	packusdwx8,@function
packusdwx8:                             # @packusdwx8
# BB#0:
	packusdw	%xmm1, %xmm0
	ret
.Ltmp172:
	.size	packusdwx8, .Ltmp172-packusdwx8

	.section	.text.abs_i8x16,"axG",@progbits,abs_i8x16,comdat
	.weak	abs_i8x16
	.type	abs_i8x16,@function
abs_i8x16:                              # @abs_i8x16
	.cfi_startproc
# BB#0:
	pabsb	%xmm0, %xmm0
	ret
.Ltmp173:
	.size	abs_i8x16, .Ltmp173-abs_i8x16
	.cfi_endproc

	.section	.text.abs_i16x8,"axG",@progbits,abs_i16x8,comdat
	.weak	abs_i16x8
	.type	abs_i16x8,@function
abs_i16x8:                              # @abs_i16x8
	.cfi_startproc
# BB#0:
	pabsw	%xmm0, %xmm0
	ret
.Ltmp174:
	.size	abs_i16x8, .Ltmp174-abs_i16x8
	.cfi_endproc

	.section	.text.abs_i32x4,"axG",@progbits,abs_i32x4,comdat
	.weak	abs_i32x4
	.type	abs_i32x4,@function
abs_i32x4:                              # @abs_i32x4
	.cfi_startproc
# BB#0:
	pabsd	%xmm0, %xmm0
	ret
.Ltmp175:
	.size	abs_i32x4, .Ltmp175-abs_i32x4
	.cfi_endproc

	.section	.text.sqrt_f32x4,"axG",@progbits,sqrt_f32x4,comdat
	.weak	sqrt_f32x4
	.type	sqrt_f32x4,@function
sqrt_f32x4:                             # @sqrt_f32x4
	.cfi_startproc
# BB#0:
	sqrtps	%xmm0, %xmm0
	ret
.Ltmp176:
	.size	sqrt_f32x4, .Ltmp176-sqrt_f32x4
	.cfi_endproc

	.section	.text.sqrt_f64x2,"axG",@progbits,sqrt_f64x2,comdat
	.weak	sqrt_f64x2
	.type	sqrt_f64x2,@function
sqrt_f64x2:                             # @sqrt_f64x2
	.cfi_startproc
# BB#0:
	sqrtpd	%xmm0, %xmm0
	ret
.Ltmp177:
	.size	sqrt_f64x2, .Ltmp177-sqrt_f64x2
	.cfi_endproc

	.section	.text.floor_f32x4,"axG",@progbits,floor_f32x4,comdat
	.weak	floor_f32x4
	.type	floor_f32x4,@function
floor_f32x4:                            # @floor_f32x4
	.cfi_startproc
# BB#0:
	roundps	$1, %xmm0, %xmm0
	ret
.Ltmp178:
	.size	floor_f32x4, .Ltmp178-floor_f32x4
	.cfi_endproc

	.section	.text.floor_f64x2,"axG",@progbits,floor_f64x2,comdat
	.weak	floor_f64x2
	.type	floor_f64x2,@function
floor_f64x2:                            # @floor_f64x2
	.cfi_startproc
# BB#0:
	roundpd	$1, %xmm0, %xmm0
	ret
.Ltmp179:
	.size	floor_f64x2, .Ltmp179-floor_f64x2
	.cfi_endproc

	.section	.text.ceil_f32x4,"axG",@progbits,ceil_f32x4,comdat
	.weak	ceil_f32x4
	.type	ceil_f32x4,@function
ceil_f32x4:                             # @ceil_f32x4
	.cfi_startproc
# BB#0:
	roundps	$2, %xmm0, %xmm0
	ret
.Ltmp180:
	.size	ceil_f32x4, .Ltmp180-ceil_f32x4
	.cfi_endproc

	.section	.text.ceil_f64x2,"axG",@progbits,ceil_f64x2,comdat
	.weak	ceil_f64x2
	.type	ceil_f64x2,@function
ceil_f64x2:                             # @ceil_f64x2
	.cfi_startproc
# BB#0:
	roundpd	$2, %xmm0, %xmm0
	ret
.Ltmp181:
	.size	ceil_f64x2, .Ltmp181-ceil_f64x2
	.cfi_endproc

	.section	.text.round_f32x4,"axG",@progbits,round_f32x4,comdat
	.weak	round_f32x4
	.type	round_f32x4,@function
round_f32x4:                            # @round_f32x4
	.cfi_startproc
# BB#0:
	roundps	$0, %xmm0, %xmm0
	ret
.Ltmp182:
	.size	round_f32x4, .Ltmp182-round_f32x4
	.cfi_endproc

	.section	.text.round_f64x2,"axG",@progbits,round_f64x2,comdat
	.weak	round_f64x2
	.type	round_f64x2,@function
round_f64x2:                            # @round_f64x2
	.cfi_startproc
# BB#0:
	roundpd	$0, %xmm0, %xmm0
	ret
.Ltmp183:
	.size	round_f64x2, .Ltmp183-round_f64x2
	.cfi_endproc

	.section	.rodata.cst16,"aM",@progbits,16
	.align	16
.LCPI97_0:
	.long	0                       # 0x0
	.long	1                       # 0x1
	.long	2                       # 0x2
	.long	3                       # 0x3
.LCPI97_1:
	.long	4                       # 0x4
	.long	5                       # 0x5
	.long	6                       # 0x6
	.long	7                       # 0x7
.LCPI97_2:
	.long	1                       # 0x1
	.long	1                       # 0x1
	.long	1                       # 0x1
	.long	1                       # 0x1
.LCPI97_3:
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	0                       # 0x0
	.byte	128                     # 0x80
	.byte	4                       # 0x4
	.byte	128                     # 0x80
	.byte	8                       # 0x8
	.byte	128                     # 0x80
	.byte	12                      # 0xc
	.byte	128                     # 0x80
.LCPI97_4:
	.byte	0                       # 0x0
	.byte	128                     # 0x80
	.byte	4                       # 0x4
	.byte	128                     # 0x80
	.byte	8                       # 0x8
	.byte	128                     # 0x80
	.byte	12                      # 0xc
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
	.byte	128                     # 0x80
.LCPI97_5:
	.long	1099431936              # float 1.700000e+01
	.long	1099431936              # float 1.700000e+01
	.long	1099431936              # float 1.700000e+01
	.long	1099431936              # float 1.700000e+01
.LCPI97_6:
	.long	1065353216              # float 1.000000e+00
	.long	1065353216              # float 1.000000e+00
	.long	1065353216              # float 1.000000e+00
	.long	1065353216              # float 1.000000e+00
	.text
	.globl	f2
	.align	16, 0x90
	.type	f2,@function
f2:                                     # @f2
	.cfi_startproc
# BB#0:                                 # %entry
	pushq	%rbp
.Ltmp191:
	.cfi_def_cfa_offset 16
	pushq	%r15
.Ltmp192:
	.cfi_def_cfa_offset 24
	pushq	%r14
.Ltmp193:
	.cfi_def_cfa_offset 32
	pushq	%r13
.Ltmp194:
	.cfi_def_cfa_offset 40
	pushq	%r12
.Ltmp195:
	.cfi_def_cfa_offset 48
	pushq	%rbx
.Ltmp196:
	.cfi_def_cfa_offset 56
	subq	$104, %rsp
.Ltmp197:
	.cfi_def_cfa_offset 160
.Ltmp198:
	.cfi_offset %rbx, -56
.Ltmp199:
	.cfi_offset %r12, -48
.Ltmp200:
	.cfi_offset %r13, -40
.Ltmp201:
	.cfi_offset %r14, -32
.Ltmp202:
	.cfi_offset %r15, -24
.Ltmp203:
	.cfi_offset %rbp, -16
	cmpl	$1, 36(%rdi)
	jne	.LBB97_1
# BB#3:                                 # %after_assert
	movl	20(%rdi), %ecx
	leal	7(%rcx), %edx
	movl	%edx, %eax
	andl	$-8, %eax
	cmpl	%ecx, %eax
	jle	.LBB97_5
# BB#4:                                 # %assert_failed1
	leaq	__unnamed_1(%rip), %rdi
	jmp	.LBB97_2
.LBB97_1:                               # %assert_failed
	leaq	__unnamed_2(%rip), %rdi
.LBB97_2:                               # %assert_failed
	addq	$104, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	jmp	halide_error@PLT        # TAILCALL
.LBB97_5:                               # %after_assert2
	movl	%edx, 44(%rsp)          # 4-byte Spill
	movl	52(%rdi), %r12d
	movl	%r12d, 40(%rsp)         # 4-byte Spill
	leal	-1(%r12,%rax), %eax
	sarl	%eax
	movl	%eax, 56(%rsp)          # 4-byte Spill
	sarl	%r12d
	subl	%r12d, %eax
	leal	1(%rax), %ecx
	movl	%ecx, 52(%rsp)          # 4-byte Spill
	movq	(%rdi), %r13
	movl	$5, %r15d
	cmpl	$6, %ecx
	cmovll	%ecx, %r15d
	leal	(%r15,%r12), %ebx
	movl	%ebx, 48(%rsp)          # 4-byte Spill
	leal	4(,%rax,4), %edi
	movq	%rdi, 32(%rsp)          # 8-byte Spill
	callq	halide_malloc@PLT
	movq	%rax, %r14
	cmpl	%ebx, %r12d
	jge	.LBB97_8
# BB#6:                                 # %f0.v0_loop.preheader
	leal	-2(%r12), %eax
	subl	56(%rsp), %eax          # 4-byte Folded Reload
	movl	$-6, %ebp
	cmpl	$-7, %eax
	cmovgl	%eax, %ebp
	notl	%ebp
	xorl	%ebx, %ebx
	.align	16, 0x90
.LBB97_7:                               # %f0.v0_loop
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%rbx), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	sinf@PLT
	movslq	%ebx, %rbx
	movss	%xmm0, (%r14,%rbx,4)
	incl	%ebx
	cmpl	%ebx, %ebp
	jne	.LBB97_7
.LBB97_8:                               # %f0.v0_after_loop
	movl	52(%rsp), %eax          # 4-byte Reload
	subl	%r15d, %eax
	xorl	%ebx, %ebx
	addl	$-5, %eax
	cmovsl	%ebx, %eax
	movl	48(%rsp), %ecx          # 4-byte Reload
	addl	%ecx, %eax
	movl	%eax, 52(%rsp)          # 4-byte Spill
	cmpl	%eax, %ecx
	jge	.LBB97_11
# BB#9:                                 # %f0.v0_loop10.preheader
	leal	-2(%r12), %eax
	movl	56(%rsp), %ecx          # 4-byte Reload
	subl	%ecx, %eax
	movl	$-6, %ebp
	cmpl	$-7, %eax
	cmovgl	%eax, %ebp
	leal	-3(%rcx,%rbp), %eax
	subl	%r12d, %eax
	cmovnsl	%eax, %ebx
	notl	%ebp
	.align	16, 0x90
.LBB97_10:                              # %f0.v0_loop10
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%rbp), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	sinf@PLT
	movslq	%ebp, %rbp
	movss	%xmm0, (%r14,%rbp,4)
	incl	%ebp
	decl	%ebx
	jne	.LBB97_10
.LBB97_11:                              # %f0.v0_after_loop11
	movl	56(%rsp), %eax          # 4-byte Reload
	leal	1(%rax), %eax
	movl	%eax, 28(%rsp)          # 4-byte Spill
	cmpl	%eax, 52(%rsp)          # 4-byte Folded Reload
	jge	.LBB97_14
# BB#12:                                # %f0.v0_loop19.preheader
	leal	-2(%r12), %ecx
	movl	56(%rsp), %edx          # 4-byte Reload
	subl	%edx, %ecx
	movl	$-6, %eax
	cmpl	$-7, %ecx
	cmovgl	%ecx, %eax
	leal	-3(%rdx,%rax), %ebx
	xorl	%ecx, %ecx
	subl	%r12d, %ebx
	cmovsl	%ecx, %ebx
	movl	%r12d, %ebp
	subl	%edx, %ebp
	decl	%ebx
	subl	%eax, %ebx
	.align	16, 0x90
.LBB97_13:                              # %f0.v0_loop19
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%rbx), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	sinf@PLT
	movslq	%ebx, %rbx
	movss	%xmm0, (%r14,%rbx,4)
	leal	1(%rbp,%rbx), %eax
	incl	%ebx
	cmpl	$1, %eax
	jne	.LBB97_13
.LBB97_14:                              # %f0.v0_after_loop20
	movq	32(%rsp), %rdi          # 8-byte Reload
	callq	halide_malloc@PLT
	movq	%rax, %rbx
	cmpl	48(%rsp), %r12d         # 4-byte Folded Reload
	jge	.LBB97_17
# BB#15:                                # %f1.v0_loop.preheader
	leal	-2(%r12), %eax
	subl	56(%rsp), %eax          # 4-byte Folded Reload
	movl	$-6, %r15d
	cmpl	$-7, %eax
	cmovgl	%eax, %r15d
	notl	%r15d
	xorl	%ebp, %ebp
	.align	16, 0x90
.LBB97_16:                              # %f1.v0_loop
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%rbp), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	cosf@PLT
	movslq	%ebp, %rbp
	movss	%xmm0, (%rbx,%rbp,4)
	incl	%ebp
	cmpl	%ebp, %r15d
	jne	.LBB97_16
.LBB97_17:                              # %f1.v0_after_loop
	movl	48(%rsp), %eax          # 4-byte Reload
	cmpl	52(%rsp), %eax          # 4-byte Folded Reload
	jge	.LBB97_20
# BB#18:                                # %f1.v0_loop32.preheader
	leal	-2(%r12), %eax
	movl	56(%rsp), %ecx          # 4-byte Reload
	subl	%ecx, %eax
	movl	$-6, %r15d
	cmpl	$-7, %eax
	cmovgl	%eax, %r15d
	leal	-3(%rcx,%r15), %ebp
	xorl	%eax, %eax
	subl	%r12d, %ebp
	cmovsl	%eax, %ebp
	notl	%r15d
	.align	16, 0x90
.LBB97_19:                              # %f1.v0_loop32
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%r15), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	cosf@PLT
	movslq	%r15d, %r15
	movss	%xmm0, (%rbx,%r15,4)
	incl	%r15d
	decl	%ebp
	jne	.LBB97_19
.LBB97_20:                              # %f1.v0_after_loop33
	movl	28(%rsp), %eax          # 4-byte Reload
	cmpl	%eax, 52(%rsp)          # 4-byte Folded Reload
	jge	.LBB97_23
# BB#21:                                # %f1.v0_loop41.preheader
	leal	-2(%r12), %ecx
	movl	56(%rsp), %edx          # 4-byte Reload
	subl	%edx, %ecx
	movl	$-6, %eax
	cmpl	$-7, %ecx
	cmovgl	%ecx, %eax
	leal	-3(%rdx,%rax), %ebp
	xorl	%ecx, %ecx
	subl	%r12d, %ebp
	cmovsl	%ecx, %ebp
	movl	%r12d, %r15d
	subl	%edx, %r15d
	decl	%ebp
	subl	%eax, %ebp
	.align	16, 0x90
.LBB97_22:                              # %f1.v0_loop41
                                        # =>This Inner Loop Header: Depth=1
	leal	(%r12,%rbp), %eax
	xorps	%xmm0, %xmm0
	cvtsi2ss	%eax, %xmm0
	callq	cosf@PLT
	movslq	%ebp, %rbp
	movss	%xmm0, (%rbx,%rbp,4)
	leal	1(%r15,%rbp), %eax
	incl	%ebp
	cmpl	$1, %eax
	jne	.LBB97_22
.LBB97_23:                              # %f1.v0_after_loop42
	movl	44(%rsp), %ebp          # 4-byte Reload
	sarl	$3, %ebp
	testl	%ebp, %ebp
	movl	40(%rsp), %r8d          # 4-byte Reload
	jle	.LBB97_26
# BB#24:                                # %f2.v0.v0_loop.preheader
	movd	%r12d, %xmm0
	pshufd	$0, %xmm0, %xmm2        # xmm2 = xmm0[0,0,0,0]
	movl	$3, %ecx
	movdqa	.LCPI97_0(%rip), %xmm8
	movdqa	.LCPI97_1(%rip), %xmm9
	movdqa	.LCPI97_2(%rip), %xmm12
	pxor	%xmm13, %xmm13
	movdqa	.LCPI97_3(%rip), %xmm10
	movdqa	.LCPI97_4(%rip), %xmm11
	movaps	.LCPI97_5(%rip), %xmm14
	movaps	.LCPI97_6(%rip), %xmm15
	.align	16, 0x90
.LBB97_25:                              # %f2.v0.v0_loop
                                        # =>This Inner Loop Header: Depth=1
	leal	-3(%r8,%rcx), %eax
	movd	%eax, %xmm0
	pshufd	$0, %xmm0, %xmm0        # xmm0 = xmm0[0,0,0,0]
	movdqa	%xmm0, %xmm1
	paddd	%xmm8, %xmm1
	movdqa	%xmm1, %xmm3
	psrad	$1, %xmm3
	paddd	%xmm9, %xmm0
	movdqa	%xmm0, %xmm4
	pand	%xmm12, %xmm4
	pcmpeqd	%xmm13, %xmm4
	psubd	%xmm2, %xmm3
	pand	%xmm12, %xmm1
	leal	3(%rcx), %eax
	leal	-2(%rcx), %edi
	leal	1(%rcx), %edx
	leal	2(%rcx), %esi
	movl	%ebp, %r12d
	leal	4(%rcx), %ebp
	pshufb	%xmm10, %xmm4
	pcmpeqd	%xmm13, %xmm1
	pshufb	%xmm11, %xmm1
	psrad	$1, %xmm0
	psubd	%xmm2, %xmm0
	movdqa	%xmm0, 80(%rsp)
	movdqa	%xmm3, 64(%rsp)
	por	%xmm4, %xmm1
	movslq	%edi, %rdi
	movslq	%ebp, %rbp
	movq	%rbp, 56(%rsp)          # 8-byte Spill
	movslq	%eax, %r9
	movslq	%esi, %r10
	movslq	%edx, %r15
	movslq	%ecx, %rax
	leal	8(%rcx), %r11d
	leal	-3(%rcx), %edx
	leal	-1(%rcx), %ecx
	movslq	%ecx, %rcx
	movslq	%edx, %rsi
	movslq	80(%rsp), %rdx
	movss	(%r14,%rdx,4), %xmm3
	movslq	64(%rsp), %rbp
	movss	(%r14,%rbp,4), %xmm4
	movss	(%rbx,%rbp,4), %xmm5
	movslq	68(%rsp), %rbp
	insertps	$16, (%rbx,%rbp,4), %xmm5
	insertps	$16, (%r14,%rbp,4), %xmm4
	movslq	72(%rsp), %rbp
	insertps	$32, (%r14,%rbp,4), %xmm4
	insertps	$32, (%rbx,%rbp,4), %xmm5
	movslq	84(%rsp), %rbp
	insertps	$16, (%r14,%rbp,4), %xmm3
	movss	(%rbx,%rdx,4), %xmm6
	insertps	$16, (%rbx,%rbp,4), %xmm6
	movslq	88(%rsp), %rbp
	insertps	$32, (%rbx,%rbp,4), %xmm6
	movslq	92(%rsp), %rdx
	insertps	$48, (%rbx,%rdx,4), %xmm6
	insertps	$32, (%r14,%rbp,4), %xmm3
	movslq	76(%rsp), %rbp
	insertps	$48, (%rbx,%rbp,4), %xmm5
	insertps	$48, (%r14,%rbp,4), %xmm4
	movl	%r12d, %ebp
	decl	%ebp
	psllw	$15, %xmm1
	psraw	$15, %xmm1
	movdqa	%xmm1, %xmm0
	punpcklbw	%xmm0, %xmm0    # xmm0 = xmm0[0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7]
	pslld	$31, %xmm0
	movaps	%xmm15, %xmm7
	divps	%xmm4, %xmm7
	mulps	%xmm14, %xmm5
	blendvps	%xmm7, %xmm5
	insertps	$48, (%r14,%rdx,4), %xmm3
	movss	%xmm5, (%r13,%rsi,4)
	punpckhbw	%xmm0, %xmm1    # xmm1 = xmm1[8],xmm0[8],xmm1[9],xmm0[9],xmm1[10],xmm0[10],xmm1[11],xmm0[11],xmm1[12],xmm0[12],xmm1[13],xmm0[13],xmm1[14],xmm0[14],xmm1[15],xmm0[15]
	extractps	$1, %xmm5, (%r13,%rdi,4)
	pslld	$31, %xmm1
	extractps	$2, %xmm5, (%r13,%rcx,4)
	extractps	$3, %xmm5, (%r13,%rax,4)
	movaps	%xmm15, %xmm4
	divps	%xmm3, %xmm4
	mulps	%xmm14, %xmm6
	movdqa	%xmm1, %xmm0
	blendvps	%xmm4, %xmm6
	movss	%xmm6, (%r13,%r15,4)
	extractps	$1, %xmm6, (%r13,%r10,4)
	extractps	$2, %xmm6, (%r13,%r9,4)
	movq	56(%rsp), %rax          # 8-byte Reload
	extractps	$3, %xmm6, (%r13,%rax,4)
	movl	%r11d, %ecx
	jne	.LBB97_25
.LBB97_26:                              # %f2.v0.v0_after_loop48
	movq	%rbx, %rdi
	callq	halide_free@PLT
	movq	%r14, %rdi
	addq	$104, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	jmp	halide_free@PLT         # TAILCALL
.Ltmp204:
	.size	f2, .Ltmp204-f2
	.cfi_endproc

	.globl	f2_jit_wrapper
	.align	16, 0x90
	.type	f2_jit_wrapper,@function
f2_jit_wrapper:                         # @f2_jit_wrapper
	.cfi_startproc
# BB#0:                                 # %entry
	movq	(%rdi), %rdi
	jmp	f2@PLT                  # TAILCALL
.Ltmp205:
	.size	f2_jit_wrapper, .Ltmp205-f2_jit_wrapper
	.cfi_endproc

	.type	halide_custom_malloc,@object # @halide_custom_malloc
	.section	.bss.halide_custom_malloc,"aGw",@nobits,halide_custom_malloc,comdat
	.weak	halide_custom_malloc
	.align	8
halide_custom_malloc:
	.quad	0
	.size	halide_custom_malloc, 8

	.type	halide_custom_free,@object # @halide_custom_free
	.section	.bss.halide_custom_free,"aGw",@nobits,halide_custom_free,comdat
	.weak	halide_custom_free
	.align	8
halide_custom_free:
	.quad	0
	.size	halide_custom_free, 8

	.type	halide_reference_clock,@object # @halide_reference_clock
	.section	.bss.halide_reference_clock,"aGw",@nobits,halide_reference_clock,comdat
	.weak	halide_reference_clock
	.align	8
halide_reference_clock:
	.zero	16
	.size	halide_reference_clock, 16

	.type	halide_error_handler,@object # @halide_error_handler
	.section	.bss.halide_error_handler,"aGw",@nobits,halide_error_handler,comdat
	.weak	halide_error_handler
	.align	8
halide_error_handler:
	.quad	0
	.size	halide_error_handler, 8

	.type	.L.str,@object          # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	 "Error: %s\n"
	.size	.L.str, 11

	.type	.L.str1,@object         # @.str1
.L.str1:
	.asciz	 "wb"
	.size	.L.str1, 3

	.type	halide_threads,@object  # @halide_threads
	.section	.bss.halide_threads,"aGw",@nobits,halide_threads,comdat
	.weak	halide_threads
	.align	4
halide_threads:
	.long	0                       # 0x0
	.size	halide_threads, 4

	.type	halide_thread_pool_initialized,@object # @halide_thread_pool_initialized
	.section	.bss.halide_thread_pool_initialized,"aGw",@nobits,halide_thread_pool_initialized,comdat
	.weak	halide_thread_pool_initialized
halide_thread_pool_initialized:
	.byte	0                       # 0x0
	.size	halide_thread_pool_initialized, 1

	.type	halide_work_queue,@object # @halide_work_queue
	.local	halide_work_queue
	.comm	halide_work_queue,2097768,8
	.type	halide_custom_do_task,@object # @halide_custom_do_task
	.section	.bss.halide_custom_do_task,"aGw",@nobits,halide_custom_do_task,comdat
	.weak	halide_custom_do_task
	.align	8
halide_custom_do_task:
	.quad	0
	.size	halide_custom_do_task, 8

	.type	halide_custom_do_par_for,@object # @halide_custom_do_par_for
	.section	.bss.halide_custom_do_par_for,"aGw",@nobits,halide_custom_do_par_for,comdat
	.weak	halide_custom_do_par_for
	.align	8
halide_custom_do_par_for:
	.quad	0
	.size	halide_custom_do_par_for, 8

	.type	.L.str2,@object         # @.str2
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str2:
	.asciz	 "HL_NUMTHREADS"
	.size	.L.str2, 14

	.type	.L.str3,@object         # @.str3
.L.str3:
	.asciz	 "HL_NUMTHREADS not defined. Defaulting to 8 threads.\n"
	.size	.L.str3, 53

	.type	_ZN12_GLOBAL__N_130pixel_type_to_tiff_sample_typeE,@object # @_ZN12_GLOBAL__N_130pixel_type_to_tiff_sample_typeE
	.section	.rodata,"a",@progbits
	.align	16
_ZN12_GLOBAL__N_130pixel_type_to_tiff_sample_typeE:
	.short	3                       # 0x3
	.short	3                       # 0x3
	.short	1                       # 0x1
	.short	2                       # 0x2
	.short	1                       # 0x1
	.short	2                       # 0x2
	.short	1                       # 0x1
	.short	2                       # 0x2
	.short	1                       # 0x1
	.short	2                       # 0x2
	.size	_ZN12_GLOBAL__N_130pixel_type_to_tiff_sample_typeE, 20

	.type	__unnamed_2,@object     # @0
	.align	16
__unnamed_2:
	.asciz	 "Static constraint violated: f2.stride.0 == 1"
	.size	__unnamed_2, 45

	.type	__unnamed_1,@object     # @1
	.align	16
__unnamed_1:
	.asciz	 "f2 is accessed out of bounds in dimension 0"
	.size	__unnamed_1, 44


	.section	".note.GNU-stack","",@progbits
