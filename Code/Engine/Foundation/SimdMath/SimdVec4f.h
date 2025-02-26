#pragma once

#include <Foundation/SimdMath/SimdFloat.h>
#include <Foundation/SimdMath/SimdVec4b.h>

/// \brief A 4-component SIMD vector class
class EZ_FOUNDATION_DLL ezSimdVec4f
{
public:
  EZ_DECLARE_POD_TYPE();

  ezSimdVec4f(); // [tested]

  explicit ezSimdVec4f(float fXyzw); // [tested]

  explicit ezSimdVec4f(const ezSimdFloat& fXyzw); // [tested]

  ezSimdVec4f(float x, float y, float z, float w = 1.0f); // [tested]

  ezSimdVec4f(ezInternal::QuadFloat v); // [tested]

  /// \brief Creates an ezSimdVec4f that is initialized to zero.
  [[nodiscard]] static ezSimdVec4f MakeZero(); // [tested]

  /// \brief Creates an ezSimdVec4f that is initialized to Not-A-Number (NaN).
  [[nodiscard]] static ezSimdVec4f MakeNaN(); // [tested]

  void Set(float fXyzw); // [tested]

  void Set(float x, float y, float z, float w); // [tested]

  void SetX(const ezSimdFloat& f); // [tested]
  void SetY(const ezSimdFloat& f); // [tested]
  void SetZ(const ezSimdFloat& f); // [tested]
  void SetW(const ezSimdFloat& f); // [tested]

  void SetZero(); // [tested]

  template <int N>
  void Load(const float* pFloats); // [tested]

  template <int N>
  void Store(float* pFloats) const; // [tested]

public:
  template <ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdVec4f GetReciprocal() const; // [tested]

  template <ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdVec4f GetSqrt() const; // [tested]

  template <ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdVec4f GetInvSqrt() const; // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdFloat GetLength() const; // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdFloat GetInvLength() const; // [tested]

  template <int N>
  ezSimdFloat GetLengthSquared() const; // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdFloat GetLengthAndNormalize(); // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  ezSimdVec4f GetNormalized() const; // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  void Normalize(); // [tested]

  template <int N, ezMathAcc::Enum acc = ezMathAcc::FULL>
  void NormalizeIfNotZero(const ezSimdFloat& fEpsilon = ezMath::SmallEpsilon<float>()); // [tested]

  template <int N>
  bool IsZero() const; // [tested]

  template <int N>
  bool IsZero(const ezSimdFloat& fEpsilon) const; // [tested]

  template <int N>
  bool IsNormalized(const ezSimdFloat& fEpsilon = ezMath::HugeEpsilon<float>()) const; // [tested]

  template <int N>
  bool IsNaN() const; // [tested]

  template <int N>
  bool IsValid() const; // [tested]

public:
  template <int N>
  ezSimdFloat GetComponent() const; // [tested]

  ezSimdFloat GetComponent(int i) const; // [tested]

  ezSimdFloat x() const; // [tested]
  ezSimdFloat y() const; // [tested]
  ezSimdFloat z() const; // [tested]
  ezSimdFloat w() const; // [tested]

  template <ezSwizzle::Enum s>
  ezSimdVec4f Get() const; // [tested]

  ///\brief x = this[s0], y = this[s1], z = other[s2], w = other[s3]
  template <ezSwizzle::Enum s>
  [[nodiscard]] ezSimdVec4f GetCombined(const ezSimdVec4f& other) const; // [tested]

public:
  [[nodiscard]] ezSimdVec4f operator-() const;                     // [tested]
  [[nodiscard]] ezSimdVec4f operator+(const ezSimdVec4f& v) const; // [tested]
  [[nodiscard]] ezSimdVec4f operator-(const ezSimdVec4f& v) const; // [tested]

  [[nodiscard]] ezSimdVec4f operator*(const ezSimdFloat& f) const; // [tested]
  [[nodiscard]] ezSimdVec4f operator/(const ezSimdFloat& f) const; // [tested]

  [[nodiscard]] ezSimdVec4f CompMul(const ezSimdVec4f& v) const; // [tested]

  template <ezMathAcc::Enum acc = ezMathAcc::FULL>
  [[nodiscard]] ezSimdVec4f CompDiv(const ezSimdVec4f& v) const; // [tested]

  [[nodiscard]] ezSimdVec4f CompMin(const ezSimdVec4f& rhs) const; // [tested]
  [[nodiscard]] ezSimdVec4f CompMax(const ezSimdVec4f& rhs) const; // [tested]

  [[nodiscard]] ezSimdVec4f Abs() const;      // [tested]
  [[nodiscard]] ezSimdVec4f Round() const;    // [tested]
  [[nodiscard]] ezSimdVec4f Floor() const;    // [tested]
  [[nodiscard]] ezSimdVec4f Ceil() const;     // [tested]
  [[nodiscard]] ezSimdVec4f Trunc() const;    // [tested]
  [[nodiscard]] ezSimdVec4f Fraction() const; // [tested]

  [[nodiscard]] ezSimdVec4f FlipSign(const ezSimdVec4b& vCmp) const; // [tested]

  [[nodiscard]] static ezSimdVec4f Select(const ezSimdVec4b& vCmp, const ezSimdVec4f& vTrue, const ezSimdVec4f& vFalse); // [tested]

  [[nodiscard]] static ezSimdVec4f Lerp(const ezSimdVec4f& a, const ezSimdVec4f& b, const ezSimdVec4f& t);

  ezSimdVec4f& operator+=(const ezSimdVec4f& v); // [tested]
  ezSimdVec4f& operator-=(const ezSimdVec4f& v); // [tested]

  ezSimdVec4f& operator*=(const ezSimdFloat& f); // [tested]
  ezSimdVec4f& operator/=(const ezSimdFloat& f); // [tested]

  ezSimdVec4b IsEqual(const ezSimdVec4f& rhs, const ezSimdFloat& fEpsilon) const; // [tested]

  [[nodiscard]] ezSimdVec4b operator==(const ezSimdVec4f& v) const; // [tested]
  [[nodiscard]] ezSimdVec4b operator!=(const ezSimdVec4f& v) const; // [tested]
  [[nodiscard]] ezSimdVec4b operator<=(const ezSimdVec4f& v) const; // [tested]
  [[nodiscard]] ezSimdVec4b operator<(const ezSimdVec4f& v) const;  // [tested]
  [[nodiscard]] ezSimdVec4b operator>=(const ezSimdVec4f& v) const; // [tested]
  [[nodiscard]] ezSimdVec4b operator>(const ezSimdVec4f& v) const;  // [tested]

  template <int N>
  [[nodiscard]] ezSimdFloat HorizontalSum() const; // [tested]

  template <int N>
  [[nodiscard]] ezSimdFloat HorizontalMin() const; // [tested]

  template <int N>
  [[nodiscard]] ezSimdFloat HorizontalMax() const; // [tested]

  template <int N>
  [[nodiscard]] ezSimdFloat Dot(const ezSimdVec4f& v) const; // [tested]

  ///\brief 3D cross product, w is ignored.
  [[nodiscard]] ezSimdVec4f CrossRH(const ezSimdVec4f& v) const; // [tested]

  ///\brief Generates an arbitrary vector such that Dot<3>(GetOrthogonalVector()) == 0
  [[nodiscard]] ezSimdVec4f GetOrthogonalVector() const; // [tested]

  /*[[deprecated("Use MakeZero() instead.")]]*/ [[nodiscard]] static ezSimdVec4f ZeroVector(); // [tested]

  [[nodiscard]] static ezSimdVec4f MulAdd(const ezSimdVec4f& a, const ezSimdVec4f& b, const ezSimdVec4f& c); // [tested]
  [[nodiscard]] static ezSimdVec4f MulAdd(const ezSimdVec4f& a, const ezSimdFloat& b, const ezSimdVec4f& c); // [tested]

  [[nodiscard]] static ezSimdVec4f MulSub(const ezSimdVec4f& a, const ezSimdVec4f& b, const ezSimdVec4f& c); // [tested]
  [[nodiscard]] static ezSimdVec4f MulSub(const ezSimdVec4f& a, const ezSimdFloat& b, const ezSimdVec4f& c); // [tested]

  [[nodiscard]] static ezSimdVec4f CopySign(const ezSimdVec4f& vMagnitude, const ezSimdVec4f& vSign); // [tested]

public:
  ezInternal::QuadFloat m_v;
};

#include <Foundation/SimdMath/Implementation/SimdVec4f_inl.h>

#if EZ_SIMD_IMPLEMENTATION == EZ_SIMD_IMPLEMENTATION_SSE
#  include <Foundation/SimdMath/Implementation/SSE/SSEVec4f_inl.h>
#elif EZ_SIMD_IMPLEMENTATION == EZ_SIMD_IMPLEMENTATION_FPU
#  include <Foundation/SimdMath/Implementation/FPU/FPUVec4f_inl.h>
#elif EZ_SIMD_IMPLEMENTATION == EZ_SIMD_IMPLEMENTATION_NEON
#  include <Foundation/SimdMath/Implementation/NEON/NEONVec4f_inl.h>
#else
#  error "Unknown SIMD implementation."
#endif
