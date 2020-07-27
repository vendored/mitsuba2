#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/render/fresnel.h>
#include <enoki/matrix.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * This file provides a number of utility functions for constructing and
 * analyzing Mueller matrices. Mueller matrices describe how a scattering
 * interaction modifies the polarization state of light, which is assumed to be
 * encoded as a Stokes vector.
 *
 * The meaning of a Stokes vector is only well defined together with its
 * corresponding reference basis vector that is orthogonal to the propagation
 * direction of the light beam. In other words, for light to be e.g. linearly
 * polarized with a horizontal orientation we first have to define what
 * "horizontal" actually means.
 * Another important detail is that the polarization ellipse, and thus the
 * Stokes vector, is observed from the view of the sensor, looking back along
 * the propagation direction of the light beam.
 *
 * To simplify APIs throughout Mitsuba, Stokes vectors are also implemented as
 * Mueller matrices (with only the first column having non-zero entries).
 */
NAMESPACE_BEGIN(mueller)

/**
* \brief Constructs the Mueller matrix of an ideal depolarizer
*
* \param value
*   The value of the (0, 0) element
*/
template <typename Float> MuellerMatrix<Float> depolarizer(Float value = 1.f) {
    MuellerMatrix<Float> result = ek::zero<MuellerMatrix<Float>>();
    result(0, 0) = value;
    return result;
}

/**
* \brief Constructs the Mueller matrix of an ideal absorber
*
* \param value
*     The amount of absorption.
*/
template <typename Float> MuellerMatrix<Float> absorber(Float value) {
    return value;
}

/**
* \brief Constructs the Mueller matrix of a linear polarizer
* which transmits linear polarization at 0 degrees.
*
* "Polarized Light" by Edward Collett, Ch. 5 eq. (13)
*
* \param value
*     The amount of attenuation of the transmitted component (1 corresponds
*     to an ideal polarizer).
*/
template <typename Float> MuellerMatrix<Float> linear_polarizer(Float value = 1.f) {
    Float a = value * .5f;
    return MuellerMatrix<Float>(
        a, a, 0, 0,
        a, a, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    );
}

/**
 * \brief Constructs the Mueller matrix of a linear retarder which has its fast
 * aligned vertically.
 *
 * This implements the general case with arbitrary phase shift and can be used
 * to construct the common special cases of quarter-wave and half-wave plates.
 *
 * "Polarized Light" by Edward Collett, Ch. 5 eq. (27)
 *
 * \param phase
 *     The phase difference between the fast and slow axis
 *
 */
template <typename Float> MuellerMatrix<Float> linear_retarder(Float phase) {
    Float s, c;
    std::tie(s, c) = ek::sincos(phase);
    return MuellerMatrix<Float>(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, c, -s,
        0, 0, s, c
    );
}

/**
* \brief Constructs the Mueller matrix of a linear diattenuator, which
* attenuates the electric field components at 0 and 90 degrees by
* 'x' and 'y', * respectively.
*/
template <typename Float> MuellerMatrix<Float> diattenuator(Float x, Float y) {
    Float a = .5f * (x + y),
          b = .5f * (x - y),
          c = ek::sqrt(x * y);

    return MuellerMatrix<Float>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0, c, 0,
        0, 0, 0, c
    );
}

/**
  * \brief Constructs the Mueller matrix of an ideal rotator, which performs a
  * counter-clockwise rotation of the electric field by 'theta' radians (when
  * facing the light beam from the sensor side).
  *
  * To be more precise, it rotates the reference frame of the current Stokes
  * vector. For example: horizontally linear polarized light s1 = [1,1,0,0]
  * will look like -45˚ linear polarized light s2 = R(45˚) * s1 = [1,0,-1,0]
  * after applying a rotator of +45˚ to it.
  *
  * "Polarized Light" by Edward Collett, Ch. 5 eq. (43)
  */
template <typename Float> MuellerMatrix<Float> rotator(Float theta) {
    auto [s, c] = ek::sincos(2.f * theta);
    return MuellerMatrix<Float>(
        1, 0, 0, 0,
        0, c, s, 0,
        0, -s, c, 0,
        0, 0, 0, 1
    );
}

/**
  * \brief Applies a counter-clockwise rotation to the mueller matrix
  * of a given element.
  */
template <typename Float>
MuellerMatrix<Float> rotated_element(Float theta,
                                     const MuellerMatrix<Float> &M) {
    MuellerMatrix<Float> R = rotator(theta), Rt = transpose(R);
    return Rt * M * R;
}

/**
 * \brief Reverse direction of propagation of the electric field. Also used for
 * reflecting reference frames.
 */
template <typename Float>
MuellerMatrix<Float> reverse(const MuellerMatrix<Float> &M) {
    return MuellerMatrix<Float>(
        1, 0,  0,  0,
        0, 1,  0,  0,
        0, 0, -1,  0,
        0, 0,  0, -1
    ) * M;
}

/**
 * \brief Calculates the Mueller matrix of a specular reflection at an
 * interface between two dielectrics or conductors.
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Complex-valued relative refractive index of the interface. In the real
 *      case, a value greater than 1.0 case means that the surface normal
 *      points into the region of lower density.
 */
template <typename Float, typename Eta>
MuellerMatrix<Float> specular_reflection(Float cos_theta_i, Eta eta) {
    ek::Complex<Float> a_s, a_p;

    std::tie(a_s, a_p, std::ignore, std::ignore, std::ignore) =
        fresnel_polarized(cos_theta_i, eta);

    Float sin_delta, cos_delta;
    std::tie(sin_delta, cos_delta) = sincos_arg_diff(a_s, a_p);

    Float r_s = ek::abs(ek::sqr(a_s)),
          r_p = ek::abs(ek::sqr(a_p)),
          a = .5f * (r_s + r_p),
          b = .5f * (r_s - r_p),
          c = ek::sqrt(r_s * r_p);

    ek::masked(sin_delta, ek::eq(c, 0.f)) = 0.f; // avoid issues with NaNs
    ek::masked(cos_delta, ek::eq(c, 0.f)) = 0.f;

    return MuellerMatrix<Float>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0,  c * cos_delta, c * sin_delta,
        0, 0, -c * sin_delta, c * cos_delta
    );
}

/**
 * \brief Calculates the Mueller matrix of a specular transmission at an
 * interface between two dielectrics or conductors.
 *
 * \param cos_theta_i
 *      Cosine of the angle between the surface normal and the incident ray
 *
 * \param eta
 *      Complex-valued relative refractive index of the interface. A value
 *      greater than 1.0 in the real case means that the surface normal is
 *      pointing into the region of lower density.
 */
template <typename Float>
MuellerMatrix<Float> specular_transmission(Float cos_theta_i, Float eta) {
    ek::Complex<Float> a_s, a_p;
    Float cos_theta_t, eta_it, eta_ti;

    std::tie(a_s, a_p, cos_theta_t, eta_it, eta_ti) =
        fresnel_polarized(cos_theta_i, eta);

    // Unit conversion factor
    Float factor = -eta_it * ek::select(ek::abs(cos_theta_i) > 1e-8f,
                                    cos_theta_t / cos_theta_i, 0.f);

    // Compute transmission amplitudes
    Float a_s_r = real(a_s) + 1.f,
          a_p_r = (1.f - real(a_p)) * eta_ti;

    Float t_s = ek::sqr(a_s_r),
          t_p = ek::sqr(a_p_r),
          a = .5f * factor * (t_s + t_p),
          b = .5f * factor * (t_s - t_p),
          c = factor * ek::sqrt(t_s * t_p);

    return MuellerMatrix<Float>(
        a, b, 0, 0,
        b, a, 0, 0,
        0, 0, c, 0,
        0, 0, 0, c
    );
}

/**
 * \brief Gives the reference frame basis for a Stokes vector.
 *
 * For light transport involving polarized quantities it is essential to keep
 * track of reference frames. A Stokes vector is only meaningful if we also know
 * w.r.t. which basis this state of light is observed.
 * In Mitsuba, these reference frames are never explicitly stored but instead
 * can be computed on the fly using this function.
 *
 * \param w
 *      Direction of travel for Stokes vector (normalized)
 *
 * \return
 *      The (implicitly defined) reference coordinate system basis for the
 *      Stokes vector travelling along \ref w.
 *
 */
template <typename Vector3>
Vector3 stokes_basis(const Vector3 &w) {
    auto [s, t] = coordinate_system(w);
    return s;
}

/**
 * \brief Gives the Mueller matrix that alignes the reference frames (defined by
 * their respective basis vectors) of two collinear stokes vectors.
 *
 * If we have a stokes vector s_current expressed in 'basis_current', we can
 * re-interpret it as a stokes vector rotate_stokes_basis(..) * s1 that is
 * expressed in 'basis_target' instead.
 * For example: Horizontally polarized light [1,1,0,0] in a basis [1,0,0] can be
 * interpreted as +45˚ linear polarized light [1,0,1,0] by switching to a target
 * basis [0.707, -0.707, 0].
 *
 * \param forward
 *      Direction of travel for Stokes vector (normalized)
 *
 * \param basis_current
 *      Current (normalized) Stokes basis. Must be orthogonal to \c forward.
 *
 * \param basis_target
 *      Target (normalized) Stokes basis. Must be orthogonal to \c forward.
 *
 * \return
 *      Mueller matrix that performs the desired change of reference frames.
 */
template <typename Vector3,
          typename Float = ek::value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Float>>
MuellerMatrix rotate_stokes_basis(const Vector3 &forward,
                                  const Vector3 &basis_current,
                                  const Vector3 &basis_target) {
    Float theta = unit_angle(ek::normalize(basis_current),
                             ek::normalize(basis_target));

    ek::masked(theta, ek::dot(forward, ek::cross(basis_current, basis_target)) < 0) *= -1.f;
    return rotator(theta);
}

/**
 * \brief Return the Mueller matrix for some new reference frames.
 * This version rotates the input/output frames independently.
 *
 * This operation is often used in polarized light transport when we have a
 * known Mueller matrix 'M' that operates from 'in_basis_current' to
 * 'out_basis_current' but instead want to re-express it as a Mueller matrix
 * that operates from 'in_basis_target' to 'out_basis_target'.
 *
 * \param M
 *      The current Mueller matrix that operates from \c in_basis_current to \c out_basis_current.
 *
 * \param in_forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param in_basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c in_forward.
 *
 * \param in_basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c in_forward.
 *
 * \param out_forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param out_basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c out_forward.
 *
 * \param out_basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c out_forward.
 *
 * \return
 *      New Mueller matrix that operates from \c in_basis_target to \c out_basis_target.
 */
template <typename Vector3,
          typename Float = ek::value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Float>>
MuellerMatrix rotate_mueller_basis(const MuellerMatrix &M,
                                   const Vector3 &in_forward,
                                   const Vector3 &in_basis_current,
                                   const Vector3 &in_basis_target,
                                   const Vector3 &out_forward,
                                   const Vector3 &out_basis_current,
                                   const Vector3 &out_basis_target) {
    MuellerMatrix R_in  = rotate_stokes_basis(in_forward, in_basis_current, in_basis_target);
    MuellerMatrix R_out = rotate_stokes_basis(out_forward, out_basis_current, out_basis_target);
    return R_out * M * transpose(R_in);
}

/**
 * \brief Return the Mueller matrix for some new reference frames.
 * This version applies the same rotation to the input/output frames.
 *
 * This operation is often used in polarized light transport when we have a
 * known Mueller matrix 'M' that operates from 'basis_current' to
 * 'basis_current' but instead want to re-express it as a Mueller matrix that
 * operates from 'basis_target' to 'basis_target'.
 *
 * \param M
 *      The current Mueller matrix that operates from \c basis_current to \c basis_current.
 *
 * \param forward
 *      Direction of travel for input Stokes vector (normalized)
 *
 * \param basis_current
 *      Current (normalized) input Stokes basis. Must be orthogonal to \c forward.
 *
 * \param basis_target
 *      Target (normalized) input Stokes basis. Must be orthogonal to \c forward.
 *
 * \return
 *      New Mueller matrix that operates from \c basis_target to \c basis_target.
 */
template <typename Vector3,
          typename Float = ek::value_t<Vector3>,
          typename MuellerMatrix = MuellerMatrix<Float>>
MuellerMatrix rotate_mueller_basis_collinear(const MuellerMatrix &M,
                                             const Vector3 &forward,
                                             const Vector3 &basis_current,
                                             const Vector3 &basis_target) {
    MuellerMatrix R = rotate_stokes_basis(forward, basis_current, basis_target);
    return R * M * transpose(R);
}

NAMESPACE_END(mueller)
NAMESPACE_END(mitsuba)
