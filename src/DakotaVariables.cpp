/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright (c) 2010, Sandia National Laboratories.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:        Variables
//- Description:  Class implementation
//- Owner:        Mike Eldred

#include "DakotaVariables.hpp"
#include "ProblemDescDB.hpp"
#include "RelaxedVariables.hpp"
#include "MixedVariables.hpp"
#include "dakota_data_io.hpp"
#include "dakota_data_util.hpp"
#include "dakota_system_defs.hpp"
#include "boost/functional/hash/hash.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/utility.hpp>  // for std::pair
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>

//#define REFCOUNT_DEBUG

static const char rcsId[]="@(#) $Id: DakotaVariables.cpp 7037 2010-10-23 01:18:08Z mseldre $";

BOOST_CLASS_EXPORT(Dakota::Variables)

namespace Dakota {


/** This constructor is the one which must build the base class data for all
    derived classes.  get_variables() instantiates a derived class letter
    and the derived constructor selects this base class constructor in its 
    initialization list (to avoid the recursion of the base class constructor
    calling get_variables() again).  Since the letter IS the representation, 
    its representation pointer is set to NULL (an uninitialized pointer causes
    problems in ~Variables). */
Variables::
Variables(BaseConstructor, const ProblemDescDB& problem_db,
	  const std::pair<short,short>& view):
  sharedVarsData(problem_db, view), variablesRep(NULL), referenceCount(1)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** This constructor is the one which must build the base class data for all
    derived classes.  get_variables() instantiates a derived class letter
    and the derived constructor selects this base class constructor in its 
    initialization list (to avoid the recursion of the base class constructor
    calling get_variables() again).  Since the letter IS the representation, 
    its representation pointer is set to NULL (an uninitialized pointer causes
    problems in ~Variables). */
Variables::
Variables(BaseConstructor, const SharedVariablesData& svd):
  sharedVarsData(svd), variablesRep(NULL), referenceCount(1)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** The default constructor: variablesRep is NULL in this case (a populated
    problem_db is needed to build a meaningful Variables object).  This
    makes it necessary to check for NULL in the copy constructor, assignment
    operator, and destructor. */
Variables::Variables():
  variablesRep(NULL), referenceCount(1)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables() called to build empty variables object."
       << std::endl;
#endif
}


/** This is the primary envelope constructor which uses problem_db to
    build a fully populated variables object.  It only needs to
    extract enough data to properly execute get_variables(problem_db),
    since the constructor overloaded with BaseConstructor builds the
    actual base class data inherited by the derived classes. */
Variables::Variables(const ProblemDescDB& problem_db):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables(ProblemDescDB&) called to instantiate envelope."
       << std::endl;
#endif

  // Set the rep pointer to the appropriate derived variables class
  variablesRep = get_variables(problem_db);
  if (!variablesRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** Initializes variablesRep to the appropriate derived type, as given
    by problem_db attributes.  The standard derived class constructors
    are invoked.  */
Variables* Variables::get_variables(const ProblemDescDB& problem_db)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_variables(ProblemDescDB&)." 
       << std::endl;
#endif

  std::pair<short,short> view = get_view(problem_db);

  // This get_variables version invokes the standard constructor.
  short active_view = view.first;
  switch (active_view) {
  case MIXED_ALL: case MIXED_DESIGN: case MIXED_ALEATORY_UNCERTAIN:
  case MIXED_EPISTEMIC_UNCERTAIN: case MIXED_UNCERTAIN: case MIXED_STATE:
    return new MixedVariables(problem_db, view); break;
  case RELAXED_ALL: case RELAXED_DESIGN: case RELAXED_ALEATORY_UNCERTAIN:
  case RELAXED_EPISTEMIC_UNCERTAIN: case RELAXED_UNCERTAIN: case RELAXED_STATE:
    return new RelaxedVariables(problem_db, view); break;
  default:
    Cerr << "Variables active view " << active_view << " not currently "
	 << "supported in derived Variables classes." << std::endl;
    return NULL; break;
  }
}


/** This is the alternate envelope constructor for instantiations on
    the fly.  This constructor executes get_variables(view), which
    invokes the default derived/base constructors, followed by a
    resize() based on vars_comps. */
Variables::Variables(const SharedVariablesData& svd):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables(SharedVariablesData&) called to instantiate "
       << "envelope." << std::endl;
#endif

  // for variablesRep, instantiate the appropriate derived variables class
  variablesRep = get_variables(svd);
  if (!variablesRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** Initializes variablesRep to the appropriate derived type, as given
    by view.  The default derived class constructors are invoked. */
Variables* Variables::get_variables(const SharedVariablesData& svd) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_variables()." << std::endl;
#endif

  short active_view = svd.view().first;
  switch (active_view) {
  case MIXED_ALL: case MIXED_DESIGN: case MIXED_ALEATORY_UNCERTAIN:
  case MIXED_EPISTEMIC_UNCERTAIN: case MIXED_UNCERTAIN: case MIXED_STATE:
    return new MixedVariables(svd); break;
  case RELAXED_ALL: case RELAXED_DESIGN: case RELAXED_ALEATORY_UNCERTAIN:
  case RELAXED_EPISTEMIC_UNCERTAIN: case RELAXED_UNCERTAIN: case RELAXED_STATE:
    return new RelaxedVariables(svd); break;
  default:
    Cerr << "Variables active view " << active_view << " not currently "
	 << "supported in derived Variables classes." << std::endl;
    return NULL; break;
  }
}


std::pair<short,short>
Variables::get_view(const ProblemDescDB& problem_db) const
{
  // Manage variable views (all, design, uncertain, aleatory, epistemic,
  // or state) and continuous-discrete domain types (mixed or relaxed).
  // The former are accomplished with Teuchos/Boost views, whereas the
  // latter utilize subclassing (they are not views).

  // Since this is used by the envelope, don't set variablesView
  std::pair<short,short> view; // view.first = active, view.second = inactive

  // View logic has 3 levels, listed in order of precedence:
  // 1. Explicit view selection in variables specification
  // 2. Peek at responses spec, and infer design view from optimization or
  //    calibration response data types
  // 3. For generic response data, peek at method spec and infer view

  // Domain logic has 2 levels, listed in order of precedence:
  // 1. Explicit mixed/relaxed domain selection in variables specification
  // 2. Peek at method spec and infer mixed/relaxed domain

  // ---------------------------------------------------------------
  // ACTIVE VIEW: ALL, DESIGN, UNCERTAIN, ALEATORY, EPISTEMIC, STATE
  // ---------------------------------------------------------------
  short view_spec = problem_db.get_short("variables.view"),
      domain_spec = problem_db.get_short("variables.domain");
  bool relaxed = ( domain_spec   == RELAXED_DOMAIN ||      // level 1 domain
		   ( domain_spec == DEFAULT_DOMAIN &&      // level 2 domain
		     method_domain(problem_db) == RELAXED_DOMAIN ) );
  switch (view_spec) {
  case DEFAULT_VIEW: // no view selection in variables spec -> level 2,3 view
    if (response_view(problem_db) == DESIGN_VIEW) // precedence level 2 view
      view.first = (relaxed) ? RELAXED_DESIGN : MIXED_DESIGN;
    else                                          // precedence level 3 view
      view.first = method_map(method_view(problem_db), relaxed);
    break;
  default: // view selection: ALL, DESIGN, UNCERTAIN, and STATE views
    view.first = method_map(view_spec, relaxed);
  }

  view.second = EMPTY; // inactive views only set by NestedModel
#ifdef DEBUG
  Cout << "Variables view: active_view = " << view.first
       << " inactive_view = " << view.second << std::endl;
#endif // DEBUG
  return view;
}


/** Copy constructor manages sharing of variablesRep and incrementing
    of referenceCount. */
Variables::Variables(const Variables& vars)
{
  variablesRep = vars.variablesRep;
  // Increment new (no old to decrement)
  if (variablesRep) // Check for assignment of NULL
    ++variablesRep->referenceCount;

#ifdef REFCOUNT_DEBUG
  Cout << "Variables::Variables(Variables&)" << std::endl;
  if (variablesRep)
    Cout << "variablesRep referenceCount = " << variablesRep->referenceCount
	 << std::endl;
#endif
}


/** Assignment operator decrements referenceCount for old variablesRep, assigns
    new variablesRep, and increments referenceCount for new variablesRep. */
Variables Variables::operator=(const Variables& vars)
{
  if (variablesRep != vars.variablesRep) { // normal case: old != new
    // Decrement old
    if (variablesRep) // Check for NULL
      if (--variablesRep->referenceCount == 0) 
	delete variablesRep;
    // Assign new
    variablesRep = vars.variablesRep;
  }
  // Increment new (either case: old == new or old != new)
  if (variablesRep) // Check for NULL
    ++variablesRep->referenceCount;

#ifdef REFCOUNT_DEBUG
  Cout << "Variables::operator=(Variables&)" << std::endl;
  if (variablesRep)
    Cout << "variablesRep referenceCount = " << variablesRep->referenceCount
	 << std::endl;
#endif

  return *this; // calls copy constructor since returned by value
}


/** Destructor decrements referenceCount and only deletes variablesRep
    when referenceCount reaches zero. */
Variables::~Variables()
{ 
  if (variablesRep) { // Check for NULL pointer
    // envelope only: decrement letter reference count and delete if 0
    --variablesRep->referenceCount;
#ifdef REFCOUNT_DEBUG
    Cout << "variablesRep referenceCount decremented to " 
         << variablesRep->referenceCount << std::endl;
#endif
    if (variablesRep->referenceCount == 0) {
#ifdef REFCOUNT_DEBUG
      Cout << "deleting variablesRep" << std::endl;
#endif
      delete variablesRep;
    }
  }
}


void Variables::build_active_views()
{
  if (variablesRep)
    variablesRep->build_active_views(); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual build_active_views"
	 << "() function.\nNo default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::build_inactive_views()
{
  if (variablesRep)
    variablesRep->build_inactive_views(); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual build_inactive_views"
	 << "() function.\nNo default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::inactive_view(short view2)
{
  if (variablesRep)
    variablesRep->inactive_view(view2);
  else {
    short view1 = sharedVarsData.view().first;
    // If active view is {RELAXED,MIXED}_ALL, outer level active view is
    // aggregated in inner loop all view and inactive view remains EMPTY.
    // Disallow assignment of an inactive ALL view.
    if (view1 > MIXED_ALL && view2 > MIXED_ALL) {
      sharedVarsData.inactive_view(view2);
      // View assigment checks are performed here (rather than in the
      // NestedModel ctor or within the Model recursion) so that all
      // levels of recursion are adequately addressed.
      check_view_compatibility();
      build_inactive_views();
    }
  }
}


/** Aggregate view and domain settings. */
short Variables::method_map(short view_spec, bool relaxed) const
{
  switch (view_spec) {
  case ALL_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_ALL : MIXED_ALL;             break;
  case DESIGN_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_DESIGN : MIXED_DESIGN;       break;
  case UNCERTAIN_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_UNCERTAIN : MIXED_UNCERTAIN; break;
  case ALEATORY_UNCERTAIN_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_ALEATORY_UNCERTAIN  : MIXED_ALEATORY_UNCERTAIN;
    break;
  case EPISTEMIC_UNCERTAIN_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_EPISTEMIC_UNCERTAIN : MIXED_EPISTEMIC_UNCERTAIN;
    break;
  case STATE_VIEW: // precedence level 1 view selection
    return (relaxed) ? RELAXED_STATE : MIXED_STATE;         break;
  default:
    Cerr << "Error: unsupported view selection in Variables::method_map()"
	 << std::endl;
    abort_handler(-1);                                      break;
  }
}


short Variables::method_domain(const ProblemDescDB& problem_db) const
{
  // B&B employs relaxation, all other employ mixed continuous-discrete
  return (problem_db.get_string("method.algorithm") == "branch_and_bound") ?
    RELAXED_DOMAIN : MIXED_DOMAIN;
}


short Variables::method_view(const ProblemDescDB& problem_db) const
{
  // last resort: active view if no user spec and no responses inference

  const String& method_name = problem_db.get_string("method.algorithm");
  if ( strends(method_name, "_parameter_study") || method_name == "dace" ||
       strbegins(method_name, "fsu_") || strbegins(method_name, "psuade_") )
    return ALL_VIEW;
  else if (strbegins(method_name, "nond_")) {
    if (method_name == "nond_sampling") { // MC/LHS, Incremental MC/LHS
      size_t num_auv = problem_db.get_sizet("variables.aleatory_uncertain"),
	     num_euv = problem_db.get_sizet("variables.epistemic_uncertain");
      if (num_auv && num_euv) return UNCERTAIN_VIEW;
      else if (num_euv)       return EPISTEMIC_UNCERTAIN_VIEW;
      else if (num_auv)       return ALEATORY_UNCERTAIN_VIEW;
      else {
	Cerr << "Error: uncertain variables required for nond_sampling in "
	     << "Variables::get_view()." << std::endl;
	abort_handler(-1);    return DEFAULT_VIEW;
      }
    }
    else if (strends(method_name, "_evidence") || 
	     strends(method_name, "_interval_est"))
      return EPISTEMIC_UNCERTAIN_VIEW;
    else // stoch exp, reliability, and efficient subspace methods
      return ALEATORY_UNCERTAIN_VIEW;
  }
  else if (method_name == "richardson_extrap") return STATE_VIEW;
  else                                         return DESIGN_VIEW;
}


short Variables::response_view(const ProblemDescDB& problem_db) const
{
  // if optimization or calibration response set, infer an active design view
  return (problem_db.get_sizet("responses.num_least_squares_terms") ||
	  problem_db.get_sizet("responses.num_objective_functions")) ?
    DESIGN_VIEW : DEFAULT_VIEW;
}


void Variables::check_view_compatibility()
{
  const std::pair<short,short>& view = sharedVarsData.view();

  short active_view = view.first, inactive_view = view.second;
#ifdef DEBUG
  Cout << "Variables view update: active_view = " << active_view
       << " inactive_view = " << inactive_view << std::endl;
#endif // DEBUG

  // check assigned active and inactive views for compatibility
  bool error_flag = false;

  // don't allow any combination of RELAXED and MIXED
  if ( ( ( active_view == RELAXED_ALL ||
	   ( active_view >= RELAXED_DESIGN &&
	     active_view <= RELAXED_STATE ) ) &&
	 ( inactive_view == MIXED_ALL ||
	   ( inactive_view >= MIXED_DESIGN &&
	     inactive_view <= MIXED_STATE ) ) ) ||
       ( ( active_view == MIXED_ALL ||
	   ( active_view >= MIXED_DESIGN && active_view <= MIXED_STATE ) ) &&
	 ( inactive_view == RELAXED_ALL ||
	   ( inactive_view >= RELAXED_DESIGN &&
	     inactive_view <= RELAXED_STATE ) ) ) ) {
    Cerr << "Error: subModel active and inactive views are inconsistent in "
	 << "RELAXED/MIXED definition in Variables::check_view_compatibility()."
	 << std::endl;
    error_flag = true;
  }

  // don't overlap any types as both active and inactive
  if ( active_view == inactive_view ||
       // don't overlap ALL and DISTINCT
       ( ( active_view == RELAXED_ALL || active_view == MIXED_ALL ) &&
	 inactive_view >= RELAXED_DESIGN && inactive_view <= MIXED_STATE ) ||
       ( active_view >= RELAXED_DESIGN && active_view <= MIXED_STATE &&
	 ( inactive_view == RELAXED_ALL || inactive_view == MIXED_ALL ) ) ||
       // don't overlap aggregated and separated UNCERTAIN
       ( ( active_view   == RELAXED_UNCERTAIN ||
	   active_view   == MIXED_UNCERTAIN ) &&
	 ( inactive_view == RELAXED_ALEATORY_UNCERTAIN  ||
	   inactive_view == RELAXED_EPISTEMIC_UNCERTAIN ||
	   inactive_view == MIXED_ALEATORY_UNCERTAIN   ||
	   inactive_view == MIXED_EPISTEMIC_UNCERTAIN ) ) ||
       ( ( active_view   == RELAXED_ALEATORY_UNCERTAIN  ||
	   active_view   == RELAXED_EPISTEMIC_UNCERTAIN ||
	   active_view   == MIXED_ALEATORY_UNCERTAIN   ||
	   active_view   == MIXED_EPISTEMIC_UNCERTAIN ) &&
	 ( inactive_view == RELAXED_UNCERTAIN ||
	   inactive_view == MIXED_UNCERTAIN ) ) ) {
    Cerr << "Error: subModel active and inactive views must not overlap in "
	 << "Variables::check_view_compatibility()." << std::endl;
    error_flag = true;
  }

  if (error_flag)
    abort_handler(-1);
}

  
template<class Archive> 
void Variables::load(Archive& ar, const unsigned int version)
{
  // Binary version.
  std::pair<short,short> view;
  ar & view;
  SizetArray vars_comps_totals;
  ar & vars_comps_totals;

  SharedVariablesData svd(view, vars_comps_totals);

  if (variablesRep) { // should not occur in current usage
    if (sharedVarsData.view() != view) {
      // decrement old reference count and replace with new letter
      Cerr << "Warning: variables type mismatch in Variables::load(Archive&)."
	   << std::endl;
      if (--variablesRep->referenceCount == 0) 
	delete variablesRep;
      variablesRep = get_variables(svd);
    }
  }
  else // read from restart: variablesRep must be instantiated
    variablesRep = get_variables(svd);

  // This code block would normally be the default implementation
  // (without variablesRep forwards), but we must support creation
  // of new letters above.
  ar & variablesRep->allContinuousVars;
  StringMultiArrayView acvl = all_continuous_variable_labels();
  ar & acvl;
  ar & variablesRep->allDiscreteIntVars;
  StringMultiArrayView adivl = all_discrete_int_variable_labels();
  ar & adivl;
  ar & variablesRep->allDiscreteRealVars;
  StringMultiArrayView adrvl = all_discrete_real_variable_labels();
  ar & adrvl;

  // rebuild active/inactive views
  variablesRep->build_views();
  // types/ids not required
}


template<class Archive> 
void Variables::save(Archive& ar, const unsigned int version) const
{
  // Binary version.
  if (variablesRep)
    variablesRep->save(ar, version); // envelope fwd to letter
  else { // default implementation for letters
    ar & sharedVarsData.view();
    ar & sharedVarsData.components_totals();
    ar & allContinuousVars;
    StringMultiArrayView acvl = all_continuous_variable_labels();
    ar & acvl; 
    ar & allDiscreteIntVars;
    StringMultiArrayView adivl = all_discrete_int_variable_labels();
    ar & adivl;
    ar & allDiscreteRealVars;
    StringMultiArrayView adrvl = all_discrete_real_variable_labels();
    ar & adrvl;
    // types/ids not required
  }
}


// These shouldn't be necessary, but using to avoid static linking
// issues until can find the right Boost macro ordering
template void Variables:: 
load<boost::archive::binary_iarchive>(boost::archive::binary_iarchive& ar, 
				      const unsigned int version); 
template void Variables:: 
save<boost::archive::binary_oarchive>(boost::archive::binary_oarchive& ar, 
				      const unsigned int version) const; 


void Variables::read(std::istream& s)
{
  if (variablesRep)
    variablesRep->read(s); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual read function.\n"
         << "No default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::write(std::ostream& s) const
{
  if (variablesRep)
    variablesRep->write(s); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual write function.\n"
         << "No default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::write_aprepro(std::ostream& s) const
{
  if (variablesRep)
    variablesRep->write_aprepro(s); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual write_aprepro "
         << "function.\nNo default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::read_annotated(std::istream& s)
{
  // ASCII version for neutral file I/O.
  std::pair<short,short> view;
  s >> view.first;
  if (s.eof()) // exception handling since EOF may not be captured properly
    throw String("Empty record in Variables::read_annotated(std::istream&)");
  s >> view.second;
  size_t i, num_vc_totals = 12;
  SizetArray vars_comps_totals(num_vc_totals);
  for (i=0; i<num_vc_totals; ++i)
    s >> vars_comps_totals[i];
  SharedVariablesData svd(view, vars_comps_totals);

  if (variablesRep) { // should not occur in current usage
    if (sharedVarsData.view() != view) {
      // decrement old reference count and replace with new letter
      Cerr << "Warning: variables type mismatch in Variables::read(istream&)."
	   << std::endl;
      if (--variablesRep->referenceCount == 0) 
        delete variablesRep;
      variablesRep = get_variables(svd);
    }
  }
  else // read from neutral file: variablesRep must be instantiated
    variablesRep = get_variables(svd);

  // This code block would normally be the default implementation (without
  // variablesRep forwards), but we must support creation of new letters above.
  read_data_annotated(s, variablesRep->allContinuousVars,
		      all_continuous_variable_labels());
  read_data_annotated(s, variablesRep->allDiscreteIntVars,
		      all_discrete_int_variable_labels());
  read_data_annotated(s, variablesRep->allDiscreteRealVars,
		      all_discrete_real_variable_labels());
  // rebuild active/inactive views
  variablesRep->build_views();
  // types/ids not required
}


void Variables::write_annotated(std::ostream& s) const
{
  // ASCII version for neutral file I/O.
  if (variablesRep)
    variablesRep->write_annotated(s); // envelope fwd to letter
  else { // default implementation for letters
    const std::pair<short,short>& view = sharedVarsData.view();
    const SizetArray& vc_totals = sharedVarsData.components_totals();
    s << view.first  << ' ' << view.second << ' ';
    size_t i, num_vc_totals = vc_totals.size();
    for (i=0; i<num_vc_totals; ++i)
      s << vc_totals[i] << ' ';
    write_data_annotated(s, allContinuousVars,
			 all_continuous_variable_labels());
    write_data_annotated(s, allDiscreteIntVars,
			 all_discrete_int_variable_labels());
    write_data_annotated(s, allDiscreteRealVars,
			 all_discrete_real_variable_labels());
    // types/ids not required
    s << '\n';
  }
}


void Variables::read_tabular(std::istream& s)
{
  if (variablesRep)
    variablesRep->read_tabular(s); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual read_tabular "
         << "function.\nNo default defined at base class." << std::endl;
    abort_handler(-1);
  }
}

void Variables::write_tabular(std::ostream& s) const
{
  if (variablesRep)
    variablesRep->write_tabular(s); // envelope fwd to letter
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual write_tabular "
         << "function.\nNo default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


void Variables::read(MPIUnpackBuffer& s)
{
  // MPI buffer version: allow passing of an empty envelope.
  bool buffer_has_letter;
  s >> buffer_has_letter;
  if (buffer_has_letter) {
    std::pair<short,short> view;
    s >> view.first >> view.second;
    size_t i, num_vc_totals = 12;
    SizetArray vars_comps_totals(num_vc_totals);
    for (i=0; i<num_vc_totals; ++i)
      s >> vars_comps_totals[i];
    SharedVariablesData svd(view, vars_comps_totals);

    if (variablesRep) { // should not occur in current usage
      if (sharedVarsData.view() != view) {
	// decrement old reference count and replace with new letter
	Cerr << "Warning: variables type mismatch in "
	     << "Variables::read(MPIUnpackBuffer&)." << std::endl;
	if (--variablesRep->referenceCount == 0) 
	  delete variablesRep;
	variablesRep = get_variables(svd);
      }
    }
    else // buffer read on slaves: variablesRep must be instantiated
      variablesRep = get_variables(svd);

    // This code block would normally be the default implementation (without
    // variablesRep forwards), but we must support additional logic above/below.
    read_data(s, variablesRep->allContinuousVars,
	      all_continuous_variable_labels());
    read_data(s, variablesRep->allDiscreteIntVars,
	      all_discrete_int_variable_labels());
    read_data(s, variablesRep->allDiscreteRealVars,
	      all_discrete_real_variable_labels());
    // rebuild active/inactive views
    variablesRep->build_views();
  }
  else if (variablesRep) {
    // empty variables envelope sent across buffer; make this one match
    if (--variablesRep->referenceCount == 0)
      delete variablesRep;
    variablesRep = NULL;
  }
}


void Variables::write(MPIPackBuffer& s) const
{
  // MPI buffer version: allow passing of an empty envelope.
  bool have_letter = !is_null();
  s << have_letter;
  if (have_letter) {
    const std::pair<short,short>& view = variablesRep->sharedVarsData.view();
    const SizetArray& vc_totals
      = variablesRep->sharedVarsData.components_totals();
    s << view.first << view.second;
    size_t i, num_vc_totals = vc_totals.size();;
    for (i=0; i<num_vc_totals; ++i)
      s << vc_totals[i];
    write_data(s, variablesRep->allContinuousVars,
	       all_continuous_variable_labels());
    write_data(s, variablesRep->allDiscreteIntVars,
	       all_discrete_int_variable_labels());
    write_data(s, variablesRep->allDiscreteRealVars,
	       all_discrete_real_variable_labels());
    // types/ids not required
  }
  // else empty envelope: send nothing other than initial bool
}


/** Deep copies are used for history mechanisms such as bestVariablesArray
    and data_pairs since these must catalogue copies (and should not
    change as the representation within currentVariables changes). */
Variables Variables::copy() const
{
  // the envelope class instantiates a new envelope and a new letter and copies
  // current attributes into the new objects.

#ifdef REFCOUNT_DEBUG
  Cout << "Variables::copy() called to generate a deep copy with no "
       << "representation sharing." << std::endl;
#endif

  Variables vars; // new envelope: referenceCount=1, variablesRep=NULL

  // shallow copy of SharedVariablesData
  if (variablesRep) {
    // deep copy of Variables, shallow copy of SharedVariablesData
    vars.variablesRep = get_variables(variablesRep->sharedVarsData);

    vars.variablesRep->allContinuousVars   = variablesRep->allContinuousVars;
    vars.variablesRep->allDiscreteIntVars  = variablesRep->allDiscreteIntVars;
    vars.variablesRep->allDiscreteRealVars = variablesRep->allDiscreteRealVars;

    vars.variablesRep->build_views();
  }

  return vars;
}


void Variables::reshape(const SizetArray& vc_totals)
{
  if (variablesRep) // envelope
    variablesRep->reshape(vc_totals);
  else { // letter lacking redefinition of virtual fn.!
    Cerr << "Error: Letter lacking redefinition of virtual reshape function.\n"
	 << "       No default defined at base class." << std::endl;
    abort_handler(-1);
  }
}


/// equality operator for Variables
bool operator==(const Variables& vars1, const Variables& vars2)
{
  // this operator is a friend of Variables

  Variables *v1_rep = vars1.variablesRep, *v2_rep = vars2.variablesRep;

  // Require different rep pointers
  if ( v1_rep != v2_rep &&
         // Require identical view
       ( v1_rep->sharedVarsData.view() != v2_rep->sharedVarsData.view() ||
	 // tolerance-based equality (labels/types/ids are ignored)
	 v1_rep->allContinuousVars     != v2_rep->allContinuousVars     ||
	 v1_rep->allDiscreteIntVars    != v2_rep->allDiscreteIntVars    ||
	 v1_rep->allDiscreteRealVars   != v2_rep->allDiscreteRealVars ) )
    return false;

  return true;
}


/// binary_equal_to (since 'operator==' is not suitable for boost/hashed lookup)
bool binary_equal_to(const Variables& vars1, const Variables& vars2)
{
  // this function is a friend of Variables

  Variables *v1_rep = vars1.variablesRep, *v2_rep = vars2.variablesRep;

  if (v1_rep == v2_rep)
    return true;

  // Require identical view
  if (v1_rep->sharedVarsData.view() != v2_rep->sharedVarsData.view())
    return false;
  // Require identical content in variable array lengths and values
  // (labels/types/ids are ignored)
  size_t cv_size = v1_rep->allContinuousVars.length(),
    div_size = v1_rep->allDiscreteIntVars.length(),
    drv_size = v1_rep->allDiscreteRealVars.length();
  if (v2_rep->allContinuousVars.length()   != cv_size ||
      v2_rep->allDiscreteIntVars.length()  != div_size ||
      v2_rep->allDiscreteRealVars.length() != drv_size)
    return false;
  size_t i;
  for (i=0; i<cv_size; ++i)
    if (v2_rep->allContinuousVars[i]   != v1_rep->allContinuousVars[i])
      return false;
  for (i=0; i<div_size; ++i)
    if (v2_rep->allDiscreteIntVars[i]  != v1_rep->allDiscreteIntVars[i])
      return false;
  for (i=0; i<drv_size; ++i)
    if (v2_rep->allDiscreteRealVars[i] != v1_rep->allDiscreteRealVars[i])
      return false;

  return true;
}


/// hash_value for Variables - required by the new BMI hash_set of PRPairs
std::size_t hash_value(const Variables& vars)
{
  // this function is a friend of Variables

  std::size_t seed = 0;
  // require identical views and variables data
  Variables *v_rep = vars.variablesRep;
  boost::hash_combine(seed, v_rep->sharedVarsData.view());
  boost::hash_combine(seed, v_rep->allContinuousVars);
  boost::hash_combine(seed, v_rep->allDiscreteIntVars);
  boost::hash_combine(seed, v_rep->allDiscreteRealVars);
  return seed;
}

} // namespace Dakota
