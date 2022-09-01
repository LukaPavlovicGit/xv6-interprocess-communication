# Context switch
#
#   void swtch(struct context **old, struct context *new);
#
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch: 			   # 4(%esp) je isto kao [esp+4] ; 4(%esp), %eax - iz esp premestamo u eax po referenci
	movl 4(%esp), %eax # ax je stari kontekst
	movl 8(%esp), %edx # dx je novi kontekst

	# Save old callee-saved registers
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi

	# Switch stacks
	movl %esp, (%eax)
	movl %edx, %esp

	# Load new callee-saved registers
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret # ret nas vraca na adresu sa koje je "novi" trenutno aktivni proces pozvao swtch
