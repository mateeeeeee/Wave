	.text
	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"WaveModule"
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:                                # %entry
	subq	$56, %rsp
	.seh_stackalloc 56
	.seh_endprologue
	callq	ReadInteger
	movq	%rax, 40(%rsp)
	movq	40(%rsp), %rcx
	callq	PrintInteger
	cmpq	$10, 40(%rsp)
	setg	%cl
	callq	Assert
	movq	40(%rsp), %rax
	movq	%rax, 48(%rsp)
# %bb.1:                                # %exit
	movq	48(%rsp), %rax
	addq	$56, %rsp
	retq
	.seh_endproc
                                        # -- End function
	.addrsig
	.addrsig_sym PrintInteger
	.addrsig_sym ReadInteger
	.addrsig_sym Assert
