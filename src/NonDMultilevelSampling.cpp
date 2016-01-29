/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDMultilevelSampling
//- Description: Implementation code for NonDMultilevelSampling class
//- Owner:       Mike Eldred
//- Checked by:
//- Version:

#include "dakota_system_defs.hpp"
#include "dakota_data_io.hpp"
#include "DakotaModel.hpp"
#include "DakotaResponse.hpp"
#include "NonDMultilevelSampling.hpp"
#include "ProblemDescDB.hpp"

static const char rcsId[]="@(#) $Id: NonDMultilevelSampling.cpp 7035 2010-10-22 21:45:39Z mseldre $";


namespace Dakota {

/** This constructor is called for a standard letter-envelope iterator 
    instantiation.  In this case, set_db_list_nodes has been called and 
    probDescDB can be queried for settings from the method specification. */
NonDMultilevelSampling::
NonDMultilevelSampling(ProblemDescDB& problem_db, Model& model):
  NonDSampling(problem_db, model),
  pilotSamples(probDescDB.get_sza("method.nond.pilot_samples"))
{
  sampleType = SUBMETHOD_RANDOM;

  // check iteratedModel for model form hierarchy and/or discretization levels
  if (iteratedModel.surrogate_type() != "hierarchical") {
    Cerr << "Error: Multilevel Monte Carlo requires a hierarchical "
	 << "surrogate model specification." << std::endl;
    abort_handler(METHOD_ERROR);
  }

  /*
  // Check for model forms and solution levels
  if (iteratedModel.subordinate_models(false).size() > 1) {
    hierarchMode = true;
  }
  if (sub_model.model_type() == "simulation" &&
      sub_model.solution_levels() > 1) {
    solnCntlMode = true;
    // TO DO
  }
  */

  if (pilotSamples.empty()) maxEvalConcurrency *= 100;
  else {
    size_t i, num_ps = pilotSamples.size(), max_ps = 0;
    for (i=0; i<num_ps; ++i)
      if (pilotSamples[i] > max_ps)
	max_ps = pilotSamples[i];
    if (max_ps)
      maxEvalConcurrency *= max_ps;
  }
}


NonDMultilevelSampling::~NonDMultilevelSampling()
{ }


void NonDMultilevelSampling::resize()
{
  NonDSampling::resize();

  Cerr << "\nError: Resizing is not yet supported in method "
       << method_enum_to_string(methodName) << "." << std::endl;
  abort_handler(METHOD_ERROR);
}


void NonDMultilevelSampling::pre_run()
{
  Analyzer::pre_run();

  // TO DO
}


/** The primary run function manages the general case: a hierarchy of model 
    forms (from the ordered model fidelities within a HierarchSurrModel), 
    each of which may contain multiple discretization levels. */
void NonDMultilevelSampling::core_run()
{
  //model,
  //  surrogate hierarchical
  //    ordered_model_fidelities = 'LF' 'MF' 'HF'
  //
  // Future: include peer alternatives (1D list --> matrix)
  //         For MLMC, could seek adaptive selection of most correlated
  //         alternative (or a convex combination of alternatives).

  size_t mf_index = 0, num_mf = iteratedModel.subordinate_models(false).size();
  // TO DO: hierarchy incl peers (not peers each optionally incl hierarchy)
  //   num_mf     = iteratedModel.model_hierarchy_depth();
  //   num_peer_i = iteratedModel.model_peer_breadth(i);

  if (num_mf > 1) {
    if (iteratedModel.surrogate_model().solution_levels()) {
      // both model forms and solutions levels --> ML-MF-MC
    }
    else {
      // only model forms --> control variate MC
    }
  }
  else // only solutions levels --> traditional ML-MC
    multilevel_mc(mf_index);
}


/** This function performs "geometrical" MLMC on a single model form
    with multiple discretization levels. */
void NonDMultilevelSampling::multilevel_mc(size_t mf_index)
{
  // Formulate as a coordinated progression towards convergence, where, e.g.,
  // time step is inferred from the spatial discretization (NOT an additional
  // solution control) based on stability criteria, e.g. CFL condition.  Can
  // we reliably capture runtime estimates as part of pilot run w/i Dakota?
  // Ultimately seems desirable to support either online or offline cost
  // estimates, to allow more accurate resource allocation when possible
  // or necessary (e.g., combustion processes with expense that is highly
  // parameter dependent).
  //   model
  //     id_model = 'LF'
  //     simulation
  //       # point to state vars; ordered based on set values for h, delta-t
  //       solution_level_control = 'dssiv1'
  //       # relative cost estimates in same order as state set values
  //       # --> re-sort into map keyed by increasing cost
  //       solution_level_cost = 10 2 200

  // How to manage the set of MLMC statistics:
  // 1. Simplest: proposal is to use the mean estimator to drive the algorithm,
  //    but carry along other estimates.
  // 2. Later: could consider a refinement for converging the estimator of the
  //    variance after convergence of the mean estimator.

  // How to manage the vector of QoI:
  // 1. Worst case: select N_l based only on QoI w/ highest total variance
  //      from pilot run --> fix for all levels and don't allow switching
  //      across major iterations (possible oscillation?  Or simple overlay
  //      of resolution reqmts?)
  // 2. Better: select N_l based on convergence in aggregated variance.
  
  iteratedModel.surrogate_model(mf_index);// LF soln_lev_index not updated (yet)
  iteratedModel.truth_model(mf_index);    // HF soln_lev_index not updated (yet)

  Model& surr_model = iteratedModel.surrogate_model();
  size_t lev, num_lev = surr_model.solution_levels(), // single model form
    qoi, iter = 0, samp, new_N_l;
  SizetArray N_l, delta_N_l;
  // retrieve cost estimates across soln levels for a particular model form
  RealVector cost = surr_model.solution_level_cost(), agg_var(num_lev);
  // For moment estimation, we accumulate telescoping sums for Q^order,
  // Yi = Q^i_{HF} - Q^i_{LF}.  For computing N_l from (aggregated) estimator
  // variance, we accumulate the square of the mean estimator Y1sq = Y1^2.
  RealMatrix sum_Y1(numFunctions, num_lev), sum_Y2(numFunctions, num_lev),
             sum_Y3(numFunctions, num_lev), sum_Y4(numFunctions, num_lev),
             sum_Y1sq(numFunctions, num_lev);
  Real agg_var_l, eps_sq_div_2, sum_sqrt_var_cost, estimator_var0 = 0.;
  IntRespMCIter r_it;
  
  // Initialize for pilot sample
  N_l.assign(num_lev, 0);
  if      (pilotSamples.empty())     delta_N_l.assign(num_lev, 100); // default
  else if (pilotSamples.size() == 1) delta_N_l.assign(num_lev, pilotSamples[0]);
  else                               delta_N_l = pilotSamples;
  Cout << "\nMLMC pilot sample:\n" << delta_N_l << std::endl;

  // now converge on sample counts per level (N_l)
  while (Pecos::l1_norm(delta_N_l) && iter <= maxIterations) {
      
    // set initial surrogate responseMode and model indices for lev 0
    iteratedModel.surrogate_response_mode(UNCORRECTED_SURROGATE); // LF
    iteratedModel.surrogate_model(mf_index, 0); // solution level 0

    sum_sqrt_var_cost = 0.;
    for (lev=0; lev<num_lev; ++lev) {

      if (lev) {
	if (lev == 1) // update responseMode for levels 1:num_lev-1
	  iteratedModel.surrogate_response_mode(AGGREGATED_MODELS); // {LF,HF}
	iteratedModel.surrogate_model(mf_index, lev-1);
	iteratedModel.truth_model(mf_index,     lev);
      }
      
      // set the number of current samples from the defined increment
      numSamples = delta_N_l[lev];
      // update total samples performed for this level
      N_l[lev]  += numSamples;

      // aggregate variances across QoI for estimating N_l (justification:
      // for independent QoI, sum of QoI variances = variance of QoI sum)
      Real& agg_var_l = agg_var[lev]; // carried over from prev iter if no samp
      if (numSamples) {

	// generate new MC parameter sets
	get_parameter_sets(iteratedModel);// pull dist params from any model
	// compute allResponses from allVariables using hierarchical model
	evaluate_parameter_sets(iteratedModel, true, false);

	Real lf_fn, hf_fn, delta_fn, lf_prod, hf_prod, *sum_Y1_l = sum_Y1[lev],
	  *sum_Y2_l = sum_Y2[lev], *sum_Y3_l   = sum_Y3[lev],
	  *sum_Y4_l = sum_Y4[lev], *sum_Y1sq_l = sum_Y1sq[lev];

	// process allResponses: accumulate new samples for each qoi
	for (r_it=allResponses.begin(); r_it!=allResponses.end(); ++r_it) {
	  const RealVector& fn_vals = r_it->second.function_values();
	  for (qoi=0; qoi<numFunctions; ++qoi) {
	    lf_fn = fn_vals[qoi];
	    // sum_Y*_l are running sums across all level increments
	    if (lev) {
	      hf_fn = fn_vals[qoi+numFunctions]; delta_fn = hf_fn - lf_fn;
	      sum_Y1_l[qoi] += delta_fn; sum_Y1sq_l[qoi] += delta_fn * delta_fn;
	      hf_prod = hf_fn*hf_fn; lf_prod = lf_fn*lf_fn;
	      sum_Y2_l[qoi] += hf_prod - lf_prod; // HF^2 - LF^2
	      hf_prod *= hf_fn; lf_prod *= lf_fn;
	      sum_Y3_l[qoi] += hf_prod - lf_prod; // HF^3 - LF^3
	      hf_prod *= hf_fn; lf_prod *= lf_fn;
	      sum_Y4_l[qoi] += hf_prod - lf_prod; // HF^4 - LF^4
	    }
	    else {
	      sum_Y1_l[qoi] += lf_fn; lf_prod = lf_fn*lf_fn;
	      sum_Y2_l[qoi] += lf_prod; sum_Y1sq_l[qoi] += lf_prod; // LF^2
	      lf_prod *= lf_fn; sum_Y3_l[qoi] += lf_prod; // LF^3
	      lf_prod *= lf_fn; sum_Y4_l[qoi] += lf_prod; // LF^4
	    }
	  }
	}

	// compute estimator mean & variance from current sample accumulation:
	agg_var_l = 0.;
	for (qoi=0; qoi<numFunctions; ++qoi) {
	  Real mu_Y = sum_Y1_l[qoi] / N_l[lev];
	  // Note: precision loss in variance is difficult to avoid without
	  // storing full sample history; must accumulate Y^2 across iterations
	  // instead of (Y-mean)^2 since mean is updated on each iteration.
	  agg_var_l += sum_Y1sq_l[qoi] / N_l[lev] - mu_Y * mu_Y;
	}
      }

      sum_sqrt_var_cost += std::sqrt(agg_var_l * cost[lev]);
      if (iter == 0) estimator_var0 += agg_var_l / N_l[lev];
    }
    // compute epsilon target based on relative tolerance: total MSE = eps^2
    // which is equally apportioned (eps^2 / 2) among discretization MSE and
    // estimator variance (\Sum var_Y_l / N_l).  Since we do not know the
    // discretization error, we compute an initial estimator variance and
    // then seek to reduce it by a relative_factor <= 1.
    if (iter == 0) // eps^2 / 2 = var * relative factor
      eps_sq_div_2 = estimator_var0 * convergenceTol;

    // update targets based on variance estimates
    Real fact = sum_sqrt_var_cost / eps_sq_div_2;
    for (lev=0; lev<num_lev; ++lev) {
      // Equation 3.9 in CTR Annual Research Briefs:
      // "A multifidelity control variate approach for the multilevel Monte 
      // Carlo technique," Geraci, Eldred, Iaccarino, 2015.
      new_N_l = std::sqrt(agg_var[lev] / cost[lev]) * fact;
      delta_N_l[lev] = (new_N_l > N_l[lev]) ? new_N_l - N_l[lev] : 0;
    }
    ++iter;
    Cout << "MLMC iteration " << iter << " sample increments:\n" << delta_N_l
	 << std::endl;
  }

  // aggregate mean and variance of estimator(s)
  if (momentStats.empty()) momentStats.shapeUninitialized(4, numFunctions);
  // Final estimator result is sum of final mu_l from telescopic sum;
  // final variance of estimator is a similar sum of variances.
  for (qoi=0; qoi<numFunctions; ++qoi) {
    Real mu_Y1 = 0., mu_Y2 = 0., mu_Y3 = 0., mu_Y4 = 0.;
    for (lev=0; lev<num_lev; ++lev) {
      size_t Nl = N_l[lev];
      mu_Y1 += sum_Y1(qoi,lev) / Nl; mu_Y2 += sum_Y2(qoi,lev) / Nl;
      mu_Y3 += sum_Y3(qoi,lev) / Nl; mu_Y4 += sum_Y4(qoi,lev) / Nl;
    }
    Real cm2 = mu_Y2 - mu_Y1*mu_Y1, cm3 = mu_Y3 - mu_Y1*(3.*cm2 + mu_Y1*mu_Y1);
    momentStats(0,qoi) = mu_Y1;
    momentStats(1,qoi) = std::sqrt(cm2);
    momentStats(2,qoi) = cm3 / std::pow(cm2, 1.5);
    momentStats(3,qoi) = (mu_Y4 - mu_Y1*(4.*cm3 + mu_Y1*(6.*cm2 + mu_Y1*mu_Y1)))
                       / (cm2 * cm2) - 3.; // excess kurtosis
  }
}


void NonDMultilevelSampling::post_run(std::ostream& s)
{
  // Statistics are generated here and output in print_results() below
  //if (statsFlag) // calculate statistics on allResponses
  //  compute_statistics(allSamples, allResponses);

  Analyzer::post_run(s);
}


void NonDMultilevelSampling::print_results(std::ostream& s)
{
  if (statsFlag) {
    s << "\nStatistics based on multilevel sample set:\n"; //<< N_l; // TO DO
    print_moments(s);//print_statistics(s);
  }
}

} // namespace Dakota
