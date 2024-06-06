#include "aiwn.h"
#include <string.h>
#ifdef __x86_64__
#define A(f, a...) code_off += f(bin ? bin + code_off : NULL, a)
#if defined(_WIN32) || defined(WIN32)
void *GenFFIBinding(void *fptr, int64_t arity) {
  int64_t code_off = 0;
  uint8_t *bin = NULL;
  while (1) {
    A(X86PushReg, RBP);
    A(X86MovRegReg, RBP, RSP);
    A(X86AndImm, RSP, ~0xF);
    A(X86PushReg, R10);
    A(X86PushReg, R11);
    A(X86SubImm32, RSP, 0x20);
    A(X86MovImm, RAX, fptr);
    A(X86LeaSIB, RCX, -1, -1, RBP, 0x10);
    A(X86CallReg, RAX);
    A(X86AddImm32, RSP, 0x20);
    A(X86PopReg, R11);
    A(X86PopReg, R10);
    A(X86Leave, 0);
    A(X86Ret, arity * 8);
    if (bin)
      break;
    bin = A_MALLOC(code_off, Fs->code_heap);
    code_off = 0;
  }
  return bin;
}
void *GenFFIBindingNaked(void *fptr, int64_t arity) {
  int64_t code_off = 0;
  uint8_t *bin = NULL;
  while (1) {
    A(X86LeaSIB, RCX, -1, -1, RSP, 0x8);
    A(X86MovImm, RAX, fptr);
    A(X86JmpReg, RAX);
    if (bin)
      break;
    bin = A_MALLOC(code_off, Fs->code_heap);
    code_off = 0;
  }
  return bin;
}
#elif defined(__linux__) || defined(__FreeBSD__)
void *GenFFIBinding(void *fptr, int64_t arity) {
  int64_t code_off = 0;
  uint8_t *bin = NULL;
  int64_t reg;
  while (1) {
    A(X86PushReg, RBP);
    A(X86MovRegReg, RBP, RSP);
    A(X86SubImm32,RSP,AIWNIOS_FREG_CNT*8);
    for(reg=0;reg!=AIWNIOS_FREG_CNT;reg++) {
      A(X86MovIndirRegF64,AIWNIOS_FREG_START+reg,-1,-1,RBP,-(1+reg)*8);
	}
    A(X86AndImm, RSP, ~0xF);
    A(X86PushReg, RSI);
    A(X86PushReg, RDI);
    A(X86PushReg, R10);
    A(X86PushReg, R11);
    A(X86LeaSIB, RDI, -1, -1, RBP, 0x10);
    A(X86MovImm, RAX, fptr);
    A(X86CallReg, RAX);
    A(X86PopReg, R11);
    A(X86PopReg, R10);
    A(X86PopReg, RDI);
    A(X86PopReg, RSI);
    for(reg=0;reg!=AIWNIOS_FREG_CNT;reg++) {
      A(X86MovRegIndirF64,AIWNIOS_FREG_START+reg,-1,-1,RBP,-(1+reg)*8);
	}
    A(X86Leave, 0);
    A(X86Ret, arity * 8);
    if (bin)
      break;
    bin = A_MALLOC(code_off, Fs->code_heap);
    code_off = 0;
  }
  return bin;
}
void *GenFFIBindingNaked(void *fptr, int64_t arity) {
  int64_t code_off = 0;
  uint8_t *bin = NULL;
  while (1) {
    A(X86MovImm, RAX, fptr);
    A(X86JmpReg, RAX);
    if (bin)
      break;
    bin = A_MALLOC(code_off, Fs->code_heap);
    code_off = 0;
  }
  return bin;
}
#endif
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
void *GenFFIBinding(void *fptr, int64_t arity) {
  // 0:  stp x29,x30[sp,-16]!
  // 4:  add x0,sp,16
  // 8:  ldr x1,label
  // c:  blr x1
  // 10: ldp x29,x30[sp],16
  // 14: ret
  // 18: label: fptr
  static CHeapCtrl *code = NULL;
  if (!code) {
    code = HeapCtrlInit(NULL, Fs, 1);
  }
  int old       = SetWriteNP(0);
  int32_t *blob = A_MALLOC(0x18 + 8, code);
  blob[0]       = ARM_stpPreImmX(29, 30, 31, -16);
  blob[1]       = ARM_addImmX(0, 31, 16);
  blob[2]       = ARM_ldrLabelX(1, 0x18 - 0x8);
  blob[3]       = ARM_blr(1);
  blob[4]       = ARM_ldpPostImmX(29, 30, 31, 16);
  blob[5]       = ARM_ret();
  memcpy(blob + 6, &fptr, sizeof(void *));
  SetWriteNP(old);
  return blob;
}
void *GenFFIBindingNaked(void *fptr, int64_t arity) {
  return fptr;
}
#endif
#if defined(__riscv__) || defined(__riscv)
void *GenFFIBinding(void *fptr, int64_t arity) {
  int32_t *blob = A_MALLOC(8 * 12, Fs->code_heap);
  blob[0] = RISCV_ADDI(10, 2, 0); // 2 is stack pointer,10 is 1st argument
  blob[1] = RISCV_ADDI(2, 2, -48);
  blob[2] = RISCV_SD(1, 2, 40);                 // 1 is return address
  blob[3] = RISCV_AUIPC(6, 0);                  // 6 is 1st temporoary
  blob[4] = RISCV_LD(6, 6, 4 * (1 + (10 - 4))); //+4 for LD,+4 for JALR,+4 for
                                                //AUIPC(address starts at AUIPC)
  blob[5]               = RISCV_JALR(1, 6, 0);
  blob[6]               = RISCV_LD(1, 2, 40); // 1 is return address
  blob[7]               = RISCV_ADDI(2, 2, 48 + arity * 8);
  blob[8]               = RISCV_FMV_D_X(10, 10);
  blob[9]               = RISCV_JALR(0, 1, 0);
  *(void **)(blob + 10) = fptr; // 16 aligned
  return blob;
}
void *GenFFIBindingNaked(void *fptr, int64_t arity) {
  return fptr;
}
#endif
