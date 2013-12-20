/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright (c) 2010, Sandia National Laboratories.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:        SharedPecosApproxData
//- Description:  Base Class for Pecos polynomial approximations
//-               
//- Owner:        Mike Eldred

#ifndef SHARED_PECOS_APPROX_DATA_H
#define SHARED_PECOS_APPROX_DATA_H

#include "SharedApproxData.hpp"
#include "SharedRegressOrthogPolyApproxData.hpp"

namespace Dakota {


/// Derived approximation class for global basis polynomials.

/** The SharedPecosApproxData class provides a global approximation
    based on basis polynomials.  This includes orthogonal polynomials
    used for polynomial chaos expansions and interpolation polynomials
    used for stochastic collocation. */

class SharedPecosApproxData: public SharedApproxData
{
  //
  //- Heading: Friends
  //

  friend class PecosApproximation;

public:

  //
  //- Heading: Constructor and destructor
  //

  /// default constructor
  SharedPecosApproxData();
  /// alternate constructor
  SharedPecosApproxData(const String& approx_type,
			const UShortArray& approx_order, size_t num_vars,
			short data_order, short output_level);
  /// standard ProblemDescDB-driven constructor
  SharedPecosApproxData(ProblemDescDB& problem_db, size_t num_vars);
  /// destructor
  ~SharedPecosApproxData();

  //
  //- Heading: Member functions
  //

  /// set pecosBasisApprox.randomVarsKey
  void random_variables_key(const Pecos::BitArray& random_vars_key);

  /// set pecosBasisApprox.driverRep
  void integration_iterator(const Iterator& iterator);

  /// invoke Pecos::SharedOrthogPolyApproxData::construct_basis()
  void construct_basis(const Pecos::ShortArray& u_types,
		       const Pecos::AleatoryDistParams& adp);

  // set Pecos::SharedOrthogPolyApproxData::basisTypes
  //void basis_types(const Pecos::ShortArray& basis_types);
  // get Pecos::SharedOrthogPolyApproxData::basisTypes
  //const Pecos::ShortArray& basis_types() const;

  /// set Pecos::SharedOrthogPolyApproxData::polynomialBasis
  void polynomial_basis(const std::vector<Pecos::BasisPolynomial>& poly_basis);
  /// get Pecos::SharedOrthogPolyApproxData::polynomialBasis
  const std::vector<Pecos::BasisPolynomial>& polynomial_basis() const;

  /// return Pecos::SharedPolyApproxData::sobolIndexMap
  const Pecos::BitArrayULongMap& sobol_index_map() const;

  /// invoke Pecos::SharedOrthogPolyApproxData::cross_validation()
  void cross_validation(bool flag);
  /// invoke Pecos::SharedOrthogPolyApproxData::coefficients_norms_flag()
  void coefficients_norms_flag(bool flag);

  /// return Pecos::SharedOrthogPolyApproxData::expansion_terms()
  size_t expansion_terms() const;
  /// return Pecos::SharedOrthogPolyApproxData::expansion_order()
  const UShortArray& expansion_order() const;
  /// invokes Pecos::SharedOrthogPolyApproxData::expansion_order(UShortArray&)
  void expansion_order(const UShortArray& order);
  /// invokes Pecos::SharedOrthogPolyApproxData::increment_order()
  void increment_order();

  /// set the expansion configuration options within Pecos::SharedPolyApproxData
  void configuration_options(const Pecos::ExpansionConfigOptions& ec_options);
  /// set the basis configuration options within Pecos::SharedPolyApproxData
  void configuration_options(const Pecos::BasisConfigOptions&     bc_options);

  /// set the noise tolerance(s) for compressed sensing algorithms
  void noise_tolerance(const RealVector& noise_tol);
  /// set the L2 penalty parameter for LASSO (elastic net variant)
  void l2_penalty(Real l2_pen);

protected:

  //
  //- Heading: Virtual function redefinitions
  //

  void build();

  void rebuild();
  void pop(bool save_surr_data);
  bool restore_available();
  size_t restoration_index();
  void restore();
  size_t finalization_index(size_t i);
  void finalize();

  void store();
  void combine(short corr_type);

private:

  //
  //- Heading: Member functions
  //

  /// return pecosSharedData
  Pecos::SharedBasisApproxData& pecos_shared_data();

  /// utility to convert Dakota type string to Pecos type enumeration
  void approx_type_to_basis_type(const String& approx_type, short& basis_type);

  //
  //- Heading: Data
  //

  /// the Pecos shared approximation data
  Pecos::SharedBasisApproxData pecosSharedData;
  /// convenience pointer to derived letter within pecosSharedData
  Pecos::SharedPolyApproxData* pecosSharedDataRep;
};


inline SharedPecosApproxData::SharedPecosApproxData():
  pecosSharedDataRep(NULL)
{ }


inline SharedPecosApproxData::~SharedPecosApproxData()
{ }


inline void SharedPecosApproxData::build()
{ pecosSharedDataRep->allocate_data(); }


inline void SharedPecosApproxData::rebuild()
{ pecosSharedDataRep->increment_data(); }


inline void SharedPecosApproxData::pop(bool save_surr_data)
{ pecosSharedDataRep->decrement_data(); } // save is implied


inline bool SharedPecosApproxData::restore_available()
{ return pecosSharedDataRep->restore_available(); }


inline size_t SharedPecosApproxData::restoration_index()
{ return pecosSharedDataRep->restoration_index(); }


inline void SharedPecosApproxData::restore()
{ pecosSharedDataRep->restore_data(); }


inline size_t SharedPecosApproxData::finalization_index(size_t i)
{ return pecosSharedDataRep->finalization_index(i); }


inline void SharedPecosApproxData::finalize()
{ pecosSharedDataRep->finalize_data(); }


inline void SharedPecosApproxData::store()
{ pecosSharedDataRep->store_data(); }


inline void SharedPecosApproxData::combine(short corr_type)
{ pecosSharedDataRep->combine_data(corr_type); }


inline void SharedPecosApproxData::
random_variables_key(const Pecos::BitArray& random_vars_key)
{ pecosSharedDataRep->random_variables_key(random_vars_key); }


inline void SharedPecosApproxData::
construct_basis(const Pecos::ShortArray& u_types,
		const Pecos::AleatoryDistParams& adp)
{
  ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    construct_basis(u_types, adp);
}


/*
inline void SharedPecosApproxData::
basis_types(const Pecos::ShortArray& basis_types)
{
  ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    orthog_poly_basis_types(basis_types);
}


inline const Pecos::ShortArray& SharedPecosApproxData::basis_types() const
{
  return ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    orthog_poly_basis_types();
}
*/


inline void SharedPecosApproxData::
polynomial_basis(const std::vector<Pecos::BasisPolynomial>& poly_basis)
{
  ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    polynomial_basis(poly_basis);
}


inline const std::vector<Pecos::BasisPolynomial>& SharedPecosApproxData::
polynomial_basis() const
{
  return ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    polynomial_basis();
}


inline const Pecos::BitArrayULongMap& SharedPecosApproxData::
sobol_index_map() const
{ return pecosSharedDataRep->sobol_index_map(); }


inline void SharedPecosApproxData::coefficients_norms_flag(bool flag)
{
  ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    coefficients_norms_flag(flag);
}


inline size_t SharedPecosApproxData::expansion_terms() const
{
  return ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    expansion_terms();
}


inline const UShortArray& SharedPecosApproxData::expansion_order() const
{
  return ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    expansion_order();
}


inline void SharedPecosApproxData::expansion_order(const UShortArray& order)
{
  ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->
    expansion_order(order);
}


inline void SharedPecosApproxData::increment_order()
{ ((Pecos::SharedOrthogPolyApproxData*)pecosSharedDataRep)->increment_order(); }


inline void SharedPecosApproxData::
configuration_options(const Pecos::ExpansionConfigOptions& ec_options)
{ pecosSharedDataRep->configuration_options(ec_options); }


inline void SharedPecosApproxData::
configuration_options(const Pecos::BasisConfigOptions& bc_options)
{ pecosSharedDataRep->configuration_options(bc_options); }


inline void SharedPecosApproxData::cross_validation(bool flag)
{
  ((Pecos::SharedRegressOrthogPolyApproxData*)pecosSharedDataRep)->
    cross_validation(flag);
}


inline void SharedPecosApproxData::noise_tolerance(const RealVector& noise_tol)
{
  ((Pecos::SharedRegressOrthogPolyApproxData*)pecosSharedDataRep)->
    noise_tolerance(noise_tol);
}


inline void SharedPecosApproxData::l2_penalty(Real l2_pen)
{
  ((Pecos::SharedRegressOrthogPolyApproxData*)pecosSharedDataRep)->
    l2_penalty(l2_pen);
}


inline Pecos::SharedBasisApproxData& SharedPecosApproxData::pecos_shared_data()
{ return pecosSharedData; }

} // namespace Dakota

#endif
