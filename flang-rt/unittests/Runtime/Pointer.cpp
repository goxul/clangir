//===-- unittests/Runtime/Pointer.cpp ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Runtime/pointer.h"
#include "tools.h"
#include "gtest/gtest.h"
#include "flang-rt/runtime/descriptor.h"

using namespace Fortran::runtime;

TEST(Pointer, BasicAllocateDeallocate) {
  // REAL(4), POINTER :: p(:)
  auto p{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 1, nullptr, CFI_attribute_pointer)};
  // ALLOCATE(p(2:11))
  RTNAME(PointerSetBounds)(*p, 0, 2, 11);
  RTNAME(PointerAllocate)
  (*p, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  EXPECT_TRUE(RTNAME(PointerIsAssociated)(*p));
  EXPECT_EQ(p->Elements(), 10u);
  EXPECT_EQ(p->GetDimension(0).LowerBound(), 2);
  EXPECT_EQ(p->GetDimension(0).UpperBound(), 11);
  // DEALLOCATE(p)
  RTNAME(PointerDeallocate)
  (*p, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  EXPECT_FALSE(RTNAME(PointerIsAssociated)(*p));
}

TEST(Pointer, ApplyMoldAllocation) {
  // REAL(4), POINTER :: p
  auto m{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 0, nullptr, CFI_attribute_pointer)};
  RTNAME(PointerAllocate)
  (*m, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);

  // CLASS(*), POINTER :: p
  auto p{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 0, nullptr, CFI_attribute_pointer)};
  p->raw().elem_len = 0;
  p->raw().type = CFI_type_other;

  RTNAME(PointerApplyMold)(*p, *m);
  RTNAME(PointerAllocate)
  (*p, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);

  EXPECT_EQ(p->ElementBytes(), m->ElementBytes());
  EXPECT_EQ(p->type(), m->type());
}

TEST(Pointer, DeallocatePolymorphic) {
  // CLASS(*) :: p
  // ALLOCATE(integer::p)
  auto p{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Integer, 4},
      4, nullptr, 0, nullptr, CFI_attribute_pointer)};
  RTNAME(PointerAllocate)
  (*p, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  // DEALLOCATE(p)
  RTNAME(PointerDeallocatePolymorphic)
  (*p, nullptr, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
}

TEST(Pointer, AllocateFromScalarSource) {
  // REAL(4), POINTER :: p(:)
  auto p{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 1, nullptr, CFI_attribute_pointer)};
  // ALLOCATE(p(2:11), SOURCE=3.4)
  float sourecStorage{3.4F};
  auto s{Descriptor::Create(Fortran::common::TypeCategory::Real, 4,
      reinterpret_cast<void *>(&sourecStorage), 0, nullptr,
      CFI_attribute_pointer)};
  RTNAME(PointerSetBounds)(*p, 0, 2, 11);
  RTNAME(PointerAllocateSource)
  (*p, *s, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  EXPECT_TRUE(RTNAME(PointerIsAssociated)(*p));
  EXPECT_EQ(p->Elements(), 10u);
  EXPECT_EQ(p->GetDimension(0).LowerBound(), 2);
  EXPECT_EQ(p->GetDimension(0).UpperBound(), 11);
  EXPECT_EQ(*p->OffsetElement<float>(), 3.4F);
  p->Destroy();
}

TEST(Pointer, AllocateSourceZeroSize) {
  using Fortran::common::TypeCategory;
  // REAL(4), POINTER :: p(:)
  auto p{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 1, nullptr, CFI_attribute_pointer)};
  // REAL(4) :: s(-1:-2) = 0.
  float sourecStorage{0.F};
  const SubscriptValue extents[1]{0};
  auto s{Descriptor::Create(TypeCategory::Real, 4,
      reinterpret_cast<void *>(&sourecStorage), 1, extents,
      CFI_attribute_other)};
  // ALLOCATE(p, SOURCE=s)
  RTNAME(PointerSetBounds)(*p, 0, -1, -2);
  RTNAME(PointerAllocateSource)
  (*p, *s, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  EXPECT_TRUE(RTNAME(PointerIsAssociated)(*p));
  EXPECT_EQ(p->Elements(), 0u);
  EXPECT_EQ(p->GetDimension(0).LowerBound(), 1);
  EXPECT_EQ(p->GetDimension(0).UpperBound(), 0);
  p->Destroy();
}

TEST(Pointer, PointerAssociateRemapping) {
  using Fortran::common::TypeCategory;
  // REAL(4), POINTER :: p(:)
  StaticDescriptor<Fortran::common::maxRank, true> staticDesc;
  auto p{staticDesc.descriptor()};
  SubscriptValue extent[1]{1};
  p.Establish(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4, nullptr, 1,
      extent, CFI_attribute_pointer);
  std::size_t descSize{p.SizeInBytes()};
  EXPECT_LE(descSize, staticDesc.byteSize);
  // REAL(4), CONTIGUOUS, POINTER :: t(:,:,:)
  auto t{Descriptor::Create(TypeCode{Fortran::common::TypeCategory::Real, 4}, 4,
      nullptr, 3, nullptr, CFI_attribute_pointer)};
  RTNAME(PointerSetBounds)(*t, 0, 1, 1);
  RTNAME(PointerSetBounds)(*t, 1, 1, 1);
  RTNAME(PointerSetBounds)(*t, 2, 1, 1);
  RTNAME(PointerAllocate)(
      *t, /*hasStat=*/false, /*errMsg=*/nullptr, __FILE__, __LINE__);
  EXPECT_TRUE(RTNAME(PointerIsAssociated)(*t));
  // INTEGER(4) :: b(2,1) = [[1,1]]
  auto b{MakeArray<TypeCategory::Integer, 4>(
      std::vector<int>{2, 1}, std::vector<std::int32_t>{1, 1})};
  // p(1:1) => t
  RTNAME(PointerAssociateRemapping)(p, *t, *b, __FILE__, __LINE__);
  EXPECT_TRUE(RTNAME(PointerIsAssociated)(p));
  EXPECT_EQ(p.rank(), 1);
  EXPECT_EQ(p.Elements(), 1u);

  // Verify that the memory past the p's descriptor is not affected.
  const char *addr = reinterpret_cast<const char *>(&staticDesc);
  const char *ptr = addr + descSize;
  const char *end = addr + staticDesc.byteSize;
  while (ptr != end) {
    if (*ptr != '\0') {
      std::fprintf(stderr, "byte %zd after pointer descriptor was written\n",
          ptr - addr);
      EXPECT_EQ(*ptr, '\0');
      break;
    }
    ++ptr;
  }
  p.Destroy();
}
