//===--- CIRToCIRArgMapping.cpp - Maps to ABI-specific arguments ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file partially mimics the ClangToLLVMArgMapping class in
// clang/lib/CodeGen/CGCall.cpp. The queries are adapted to operate on the CIR
// dialect, however. This class was extracted into a separate file to resolve
// build issues.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CIR_DIALECT_TRANSFORMS_TARGETLOWERING_CIRTOCIRARGMAPPING_H
#define LLVM_CLANG_LIB_CIR_DIALECT_TRANSFORMS_TARGETLOWERING_CIRTOCIRARGMAPPING_H

#include "CIRLowerContext.h"
#include "LowerFunctionInfo.h"
#include "clang/CIR/ABIArgInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"

namespace cir {

/// Encapsulates information about the way function arguments from
/// LoweringFunctionInfo should be passed to actual CIR function.
class CIRToCIRArgMapping {
  static const unsigned InvalidIndex = ~0U;
  unsigned SRetArgNo;
  unsigned TotalIRArgs;

  /// Arguments of CIR function corresponding to single CIR argument.
  /// NOTE(cir): We add an MLIR block argument here indicating the actual
  /// argument in the IR.
  struct IRArgs {
    unsigned PaddingArgIndex;
    // Argument is expanded to IR arguments at positions
    // [FirstArgIndex, FirstArgIndex + NumberOfArgs).
    unsigned FirstArgIndex;
    unsigned NumberOfArgs;

    IRArgs()
        : PaddingArgIndex(InvalidIndex), FirstArgIndex(InvalidIndex),
          NumberOfArgs(0) {}
  };

  llvm::SmallVector<IRArgs, 8> ArgInfo;

public:
  CIRToCIRArgMapping(const CIRLowerContext &context,
                     const LowerFunctionInfo &FI, bool onlyRequiredArgs = false)
      : SRetArgNo(InvalidIndex),
        ArgInfo(onlyRequiredArgs ? FI.getNumRequiredArgs() : FI.arg_size()) {
    construct(context, FI, onlyRequiredArgs);
  };

  unsigned totalIRArgs() const { return TotalIRArgs; }

  bool hasPaddingArg(unsigned ArgNo) const {
    cir_cconv_assert(ArgNo < ArgInfo.size());
    return ArgInfo[ArgNo].PaddingArgIndex != InvalidIndex;
  }

  void construct(const CIRLowerContext &context, const LowerFunctionInfo &FI,
                 bool onlyRequiredArgs = false) {
    unsigned IRArgNo = 0;
    bool SwapThisWithSRet = false;
    const cir::ABIArgInfo &RetAI = FI.getReturnInfo();

    if (RetAI.getKind() == cir::ABIArgInfo::Indirect) {
      SwapThisWithSRet = RetAI.isSRetAfterThis();
      SRetArgNo = SwapThisWithSRet ? 1 : IRArgNo++;
    }

    unsigned ArgNo = 0;
    unsigned NumArgs =
        onlyRequiredArgs ? FI.getNumRequiredArgs() : FI.arg_size();
    for (LowerFunctionInfo::const_arg_iterator I = FI.arg_begin();
         ArgNo < NumArgs; ++I, ++ArgNo) {
      cir_cconv_assert(I != FI.arg_end());
      // Type ArgType = I->type;
      const cir::ABIArgInfo &AI = I->info;
      // Collect data about IR arguments corresponding to Clang argument ArgNo.
      auto &IRArgs = ArgInfo[ArgNo];

      if (cir::MissingFeatures::argumentPadding()) {
        cir_cconv_unreachable("NYI");
      }

      switch (AI.getKind()) {
      case cir::ABIArgInfo::Extend:
      case cir::ABIArgInfo::Direct: {
        // FIXME(cir): handle sseregparm someday...
        cir_cconv_assert(AI.getCoerceToType() && "Missing coerced type!!");
        RecordType STy = mlir::dyn_cast<RecordType>(AI.getCoerceToType());
        if (AI.isDirect() && AI.getCanBeFlattened() && STy) {
          IRArgs.NumberOfArgs = STy.getNumElements();
        } else {
          IRArgs.NumberOfArgs = 1;
        }
        break;
      }
      case cir::ABIArgInfo::Indirect:
      case cir::ABIArgInfo::IndirectAliased:
        IRArgs.NumberOfArgs = 1;
        break;

      default:
        cir_cconv_unreachable("Missing ABIArgInfo::Kind");
      }

      if (IRArgs.NumberOfArgs > 0) {
        IRArgs.FirstArgIndex = IRArgNo;
        IRArgNo += IRArgs.NumberOfArgs;
      }

      // Skip over the sret parameter when it comes second.  We already handled
      // it above.
      if (IRArgNo == 1 && SwapThisWithSRet)
        IRArgNo++;
    }
    cir_cconv_assert(ArgNo == ArgInfo.size());

    if (cir::MissingFeatures::inallocaArgs()) {
      cir_cconv_unreachable("NYI");
    }

    TotalIRArgs = IRArgNo;
  }

  /// Returns index of first IR argument corresponding to ArgNo, and their
  /// quantity.
  std::pair<unsigned, unsigned> getIRArgs(unsigned ArgNo) const {
    cir_cconv_assert(ArgNo < ArgInfo.size());
    return std::make_pair(ArgInfo[ArgNo].FirstArgIndex,
                          ArgInfo[ArgNo].NumberOfArgs);
  }

  bool hasSRetArg() const { return SRetArgNo != InvalidIndex; }

  unsigned getSRetArgNo() const {
    assert(hasSRetArg());
    return SRetArgNo;
  }
};

} // namespace cir

#endif // LLVM_CLANG_LIB_CIR_DIALECT_TRANSFORMS_TARGETLOWERING_CIRTOCIRARGMAPPING_H
