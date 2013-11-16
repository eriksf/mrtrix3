/*
   Copyright 2012 Brain Research Institute, Melbourne, Australia

   Written by David Raffelt, 24/02/2012

   This file is part of MRtrix.

   MRtrix is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   MRtrix is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef __image_registration_transform_rigid_h__
#define __image_registration_transform_rigid_h__

#include "image/registration/transform/base.h"

#include "math/vector.h"
#include "math/matrix.h"
#include "math/versor.h"
#include "math/gradient_descent.h"

using namespace MR::Math;

namespace MR
{
  namespace Image
  {
    namespace Registration
    {
      namespace Transform
      {


        class VersorUpdate {
          public:
            template <typename ValueType>
            inline bool operator() (Math::Vector<ValueType>& newx,
                                    const Math::Vector<ValueType>& x,
                                    const Math::Vector<ValueType>& g,
                                    ValueType step_size) {

              Vector<ValueType> axis (3);
              axis[0] = g[0];
              axis[1] = g[1];
              axis[2] = g[2];
              Versor<ValueType> gradient_rotation;
              gradient_rotation.set (axis, -step_size * Math::norm (axis));

              Vector<ValueType> right_part (3);
              right_part[0] = x[0];
              right_part[1] = x[1];
              right_part[2] = x[2];

              Versor<ValueType> current_rotation;
              current_rotation.set (right_part);

              Versor<ValueType> new_rotation = current_rotation * gradient_rotation;

              newx[0] = new_rotation[1];
              newx[1] = new_rotation[2];
              newx[2] = new_rotation[3];
              newx[3] = x[3] - step_size * g[3];
              newx[4] = x[4] - step_size * g[4];
              newx[5] = x[5] - step_size * g[5];

              bool changed = false;
              for (size_t n = 0; n < x.size(); ++n) {
                if (newx[n] != x[n])
                  changed = true;
              }
              return changed;
            }
        };




        class VersorUpdateTest {
          public:
            template <typename ValueType>
            inline bool operator() (Math::Vector<ValueType>& newx,
                                    const Math::Vector<ValueType>& x,
                                    Math::Vector<ValueType>& g,
                                    ValueType step_size) {
              Math::Vector<ValueType> vx (x.sub(0,4));
              Math::Vector<ValueType> vg (g.sub(0,4));

              ValueType dp = Math::dot (vx, vg);
              for (size_t n = 0; n < 4; ++n)
                vg[n] -= dp * vx[n];

              if (!Math::LinearUpdate() (newx, x, g, step_size))
                return false;

              Math::Vector<ValueType> v (newx.sub(0,4));
              Math::normalise (v);
              return newx != x;
            }
        };


        /** \addtogroup Transforms
        @{ */

        /*! A 3D rigid transformation class for registration.
         *
         * This class defines a rigid transform using 6 parameters. The first 3 parameters define rotation using a versor (unit quaternion),
         * while the last 3 parameters define translation. Note that since the versor parameters do not define a vector space, any updates
         * must be performed using a versor composition (not an addition). This can be achieved by passing the update_parameters method
         * from this class as a function pointer to the gradient_decent run method.
         *
         * This class supports the ability to define the centre of rotation. This should be set prior to commencing registration based on
         * the centre of the target image. The translation also should be initialised as moving image centre minus the target image centre.
         * This can done automatically using functions available in  src/registration/transform/initialiser.h
         *
         */
        template <typename ValueType = float>
        class Rigid: public Base<ValueType>  {
          public:

            typedef typename Base<ValueType>::ParameterType ParameterType;
            typedef VersorUpdate UpdateType;

            Rigid () : Base<ValueType> (6) {
              for (size_t i = 0; i < 3; i++)
                this->optimiser_weights[i] = 1.0;
              for (size_t i = 3; i < 6; i++)
                this->optimiser_weights[i] = 1.0;
            }

            template <class PointType>
            void get_jacobian_wrt_params (const PointType& p, Matrix<ValueType>& jacobian) const {

              const ValueType vw = versor_[0];
              const ValueType vx = versor_[1];
              const ValueType vy = versor_[2];
              const ValueType vz = versor_[3];

              jacobian.resize(3, 6);
              jacobian.zero();

              const double px = p[0] - this->centre[0];
              const double py = p[1] - this->centre[1];
              const double pz = p[2] - this->centre[2];

              const double vxx = vx * vx;
              const double vyy = vy * vy;
              const double vzz = vz * vz;
              const double vww = vw * vw;
              const double vxy = vx * vy;
              const double vxz = vx * vz;
              const double vxw = vx * vw;
              const double vyz = vy * vz;
              const double vyw = vy * vw;
              const double vzw = vz * vw;

              jacobian(0,0) = 2.0 * ( ( vyw + vxz ) * py + ( vzw - vxy ) * pz ) / vw;
              jacobian(1,0) = 2.0 * ( ( vyw - vxz ) * px   - 2 * vxw   * py + ( vxx - vww ) * pz ) / vw;
              jacobian(2,0) = 2.0 * ( ( vzw + vxy ) * px + ( vww - vxx ) * py   - 2 * vxw   * pz ) / vw;
              jacobian(0,1) = 2.0 * ( -2 * vyw  * px + ( vxw + vyz ) * py + ( vww - vyy ) * pz ) / vw;
              jacobian(1,1) = 2.0 * ( ( vxw - vyz ) * px + ( vzw + vxy ) * pz ) / vw;
              jacobian(2,1) = 2.0 * ( ( vyy - vww ) * px + ( vzw - vxy ) * py   - 2 * vyw   * pz ) / vw;
              jacobian(0,2) = 2.0 * ( -2 * vzw  * px + ( vzz - vww ) * py + ( vxw - vyz ) * pz ) / vw;
              jacobian(1,2) = 2.0 * ( ( vww - vzz ) * px   - 2 * vzw   * py + ( vyw + vxz ) * pz ) / vw;
              jacobian(2,2) = 2.0 * ( ( vxw + vyz ) * px + ( vyw - vxz ) * py ) / vw;
              jacobian(0,3) = 1.0;
              jacobian(1,4) = 1.0;
              jacobian(2,5) = 1.0;
            }

            void set_rotation (const Math::Vector<ValueType>& axis, ValueType angle) {
              versor_.set(axis, angle);
              compute_matrix();
              this->compute_offset();
            }


            void set_parameter_vector (const Math::Vector<ValueType>& param_vector) {
              Vector<ValueType> axis(3);
              double norm = param_vector[0] * param_vector[0];
              norm += param_vector[1] * param_vector[1];
              norm += param_vector[2] * param_vector[2];
              axis[0] = param_vector[0];
              axis[1] = param_vector[1];
              axis[2] = param_vector[2];
              if (norm > 0)
                norm = Math::sqrt (norm);

              double epsilon = 1e-10;
              if (norm >= 1.0 - epsilon)
                axis /= (norm + epsilon * norm);

              versor_.set (axis);
              compute_matrix();

              this->translation[0] = param_vector[3];
              this->translation[1] = param_vector[4];
              this->translation[2] = param_vector[5];
              this->compute_offset();
            }

            void get_parameter_vector (Vector<ValueType>& param_vector) const {
              param_vector.allocate (6);
              param_vector[0] = versor_[1];
              param_vector[1] = versor_[2];
              param_vector[2] = versor_[3];
              param_vector[3] = this->translation[0];
              param_vector[4] = this->translation[1];
              param_vector[5] = this->translation[2];
            }

            UpdateType* get_gradient_descent_updator (){
              return &gradient_descent_updator;
            }

          protected:

            void compute_matrix () {
              versor_.to_matrix (this->matrix);
            }

            Versor<ValueType> versor_;
            UpdateType gradient_descent_updator;
        };




        /*! A 3D rigid transformation class for registration.
         *
         * This class defines a rigid transform using 6 parameters. The first 3 parameters define rotation using a versor (unit quaternion),
         * while the last 3 parameters define translation. Note that since the versor parameters do not define a vector space, any updates
         * must be performed using a versor composition (not an addition). This can be achieved by passing the update_parameters method
         * from this class as a function pointer to the gradient_decent run method.
         *
         * This class supports the ability to define the centre of rotation. This should be set prior to commencing registration based on
         * the centre of the target image. The translation also should be initialised as moving image centre minus the target image centre.
         * This can done automatically using functions available in  src/registration/transform/initialiser.h
         *
         */
        template <typename ValueType = float>
        class RigidTest : public Base<ValueType>  {
          public:

            typedef typename Base<ValueType>::ParameterType ParameterType;
            typedef VersorUpdateTest UpdateType;

            RigidTest () : Base<ValueType> (7) {
              this->optimiser_weights_ = 1.0;
            }

            template <class PointType>
            void get_jacobian_wrt_params (const PointType& p, Matrix<ValueType>& jacobian) const {

              jacobian.resize(3,7);
              jacobian.zero();
              Vector<ValueType> v (3);
              v[0] = p[0] - this->centre_[0];
              v[1] = p[1] - this->centre_[1];
              v[2] = p[2] - this->centre_[2];

                // compute Jacobian with respect to quaternion parameters
              jacobian(0,1) =  2.0 * ( versor_[1] * v[0] + versor_[2] * v[1] + versor_[3] * v[2]);
              jacobian(0,2) =  2.0 * (-versor_[2] * v[0] + versor_[1] * v[1] + versor_[0] * v[2]);
              jacobian(0,3) =  2.0 * (-versor_[3] * v[0] - versor_[0] * v[1] + versor_[1] * v[2]);
              jacobian(0,0) = -2.0 * (-versor_[0] * v[0] + versor_[3] * v[1] - versor_[2] * v[2]);

              jacobian(1,1) = -jacobian(0,2);
              jacobian(1,2) =  jacobian(0,1);
              jacobian(1,3) =  jacobian(0,0);
              jacobian(1,0) = -jacobian(0,3);

              jacobian(2,1) = -jacobian(0,3);
              jacobian(2,2) = -jacobian(0,0);
              jacobian(2,3) =  jacobian(0,1);
              jacobian(2,0) =  jacobian(0,2);

               // compute derivatives for the translation part
              for (size_t dim = 0; dim < 3; ++dim)
                jacobian(dim, 4+dim) = 1.0;
            }

            void set_rotation (const Math::Vector<ValueType>& axis, ValueType angle) {
              versor_.set(axis, angle);
              compute_matrix();
              this->compute_offset();
            }


            void set_parameter_vector (const Math::Vector<ValueType>& param_vector) {
              versor_ = Versor<ValueType> (param_vector.sub(0,4));
              compute_matrix();
              this->translation_ = param_vector.sub(4,7);
              this->compute_offset();
            }

            void get_parameter_vector (Vector<ValueType>& param_vector) const {
              param_vector.allocate (7);
              param_vector[0] = versor_[0];
              param_vector[1] = versor_[1];
              param_vector[2] = versor_[2];
              param_vector[3] = versor_[3];
              param_vector[4] = this->translation_[0];
              param_vector[5] = this->translation_[1];
              param_vector[6] = this->translation_[2];
            }

            UpdateType* get_gradient_descent_updator (){
              return &gradient_descent_updator;
            }

          protected:

            void compute_matrix () {
              versor_.to_matrix (this->matrix_);
            }

            Versor<ValueType> versor_;
            UpdateType gradient_descent_updator;
        };

        //! @}
      }
    }
  }
}

#endif