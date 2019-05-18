//
// Created by swift on 2019/5/12.
//

#include "code_relocate_a64.h"
#include "decoder_arm64.h"

using namespace SandHook::RegistersA64;
using namespace SandHook::AsmA64;

#define __ assemblerA64->

#define IMPL_RELOCATE(X) void CodeRelocateA64::relocate_##X (INST_A64(X)* inst, void* toPc) throw(ErrorCodeException)

#define CASE(X) \
case ENUM_VALUE(InstCodeA64, InstCodeA64::X): \
relocate_##X(reinterpret_cast<INST_A64(X)*>(instruction), toPc); \
break;

CodeRelocateA64::CodeRelocateA64(AssemblerA64 &assembler) : CodeRelocate(assembler.codeContainer) {
    this->assemblerA64 = &assembler;
}

void* CodeRelocateA64::relocate(void *startPc, Addr len, void *toPc = nullptr) throw(ErrorCodeException) {
    assemblerA64->allocBufferFirst(static_cast<U32>(len * 8));
    void* curPc = assemblerA64->getPC();
    Arm64Decoder decoder = Arm64Decoder();
    if (toPc == nullptr) {
        decoder.decode(startPc, len, *this);
    } else {
        //TODO
    }
    return curPc;
}

void* CodeRelocateA64::relocate(Instruction<Base> *instruction, void *toPc) throw(ErrorCodeException) {
    void* curPc = assemblerA64->getPC();
    if (!instruction->pcRelate()) {
        __ Emit(instruction);
        return curPc;
    }
    switch (instruction->instCode()) {
        CASE(B_BL)
        CASE(B_COND)
        CASE(TBZ_TBNZ)
        CASE(CBZ_CBNZ)
        CASE(LDR_LIT)
        CASE(ADR_ADRP)
        default:
            __ Emit(instruction);
    }
    return curPc;
}

IMPL_RELOCATE(B_BL) {
    Addr targetAddr = inst->getImmPCOffsetTarget();
    if (inst->op == inst->BL) {
        __ Mov(LR, (Addr)inst->getPC() + inst->size());
    }
    __ Mov(IP1, targetAddr);
    __ Br(IP1);
}

IMPL_RELOCATE(B_COND) {

    Addr targetAddr = inst->getImmPCOffsetTarget();

    Label *true_label = new Label();
    Label *false_label = new Label();

    __ B(inst->condition, true_label);
    __ B(false_label);

    __ Emit(true_label);
    __ Mov(IP1, targetAddr);
    __ Br(IP1);

    __ Emit(false_label);
}

IMPL_RELOCATE(TBZ_TBNZ) {

    Addr targetAddr = inst->getImmPCOffsetTarget();

    Label *true_label = new Label();
    Label *false_label = new Label();

    if (inst->op == INST_A64(TBZ_TBNZ)::TBNZ) {
        __ Tbnz(*inst->rt, inst->bit, true_label);
    } else {
        __ Tbz(*inst->rt, inst->bit, true_label);
    }
    __ B(false_label);

    __ Emit(true_label);
    __ Mov(IP1, targetAddr);
    __ Br(IP1);

    __ Emit(false_label);
}

IMPL_RELOCATE(CBZ_CBNZ) {
    Addr targetAddr = inst->getImmPCOffsetTarget();

    Label *true_label = new Label();
    Label *false_label = new Label();

    if (inst->op == INST_A64(CBZ_CBNZ)::CBNZ) {
        __ Cbnz(*inst->rt, true_label);
    } else {
        __ Cbz(*inst->rt, true_label);
    }

    __ B(false_label);

    __ Emit(true_label);
    __ Mov(IP1, targetAddr);
    __ Br(IP1);

    __ Emit(false_label);
}

IMPL_RELOCATE(LDR_LIT) {
    Addr targetAddr = inst->getImmPCOffsetTarget();
    XRegister* rtX = XReg(inst->rt->getCode());
    WRegister* rtW = WReg(inst->rt->getCode());
    switch (inst->op) {
        case INST_A64(LDR_LIT)::LDR_X:
            __ Mov(*rtX, targetAddr);
            __ Ldr(*rtW, MemOperand(rtX, 0, Offset));
            break;
        case INST_A64(LDR_LIT)::LDR_W:
            __ Mov(*rtX, targetAddr);
            __ Ldr(*rtX, MemOperand(rtX, 0, Offset));
            break;
        case INST_A64(LDR_LIT)::LDR_SW:
            break;
        case INST_A64(LDR_LIT)::LDR_PRFM:
            __ Push(X0);
            __ Mov(X0, targetAddr);
            __ Ldrsw(X0, MemOperand(rtX, 0, Offset));
            __ Pop(X0);
            break;
    }
}

IMPL_RELOCATE(ADR_ADRP) {
    __ Mov(*inst->rd, inst->getImmPCOffsetTarget());
}

bool CodeRelocateA64::visit(Unit<Base> *unit, void *pc) {
    relocate(reinterpret_cast<Instruction<Base> *>(unit), assemblerA64->getPC());
    return true;
}