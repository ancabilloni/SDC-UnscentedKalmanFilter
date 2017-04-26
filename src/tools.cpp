#include <iostream>
#include "tools.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
  /**
  TODO:
    * Calculate the RMSE here.
  */
  VectorXd rmse(4);
  rmse << 0, 0, 0, 0;
  if((estimations.size() != ground_truth.size()) || (estimations.size() == 0)) {
    return rmse;
  }
  for(unsigned int i=0; i<estimations.size(); ++i){
    VectorXd sum_error = estimations[i] - ground_truth[i];
    sum_error = sum_error.array() * sum_error.array();
    rmse += sum_error;
  }
  rmse = rmse/estimations.size();
  rmse = rmse.array().sqrt();
  return rmse;

}
