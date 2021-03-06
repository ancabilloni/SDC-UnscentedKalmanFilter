#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  is_initialized_ = false;
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);
  P_ << 1, 0, 0, 0, 0,
          0, 1, 0, 0, 0,
          0, 0, 1, 0, 0,
          0, 0, 0, 100, 0,
          0, 0, 0, 0, 100;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.4;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.65;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  // Current time stamp //
  time_us_ = 0;

  // State vector size //
  n_x_ = 5;

  // Augmented vector size //
  n_aug_ = 7;

  // weight vector //
  weights_ = VectorXd(2*n_aug_+1);

  // predicted sigma points matrix //
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1);

  // NIS //
  NIS_laser_ = 0;
  NIS_radar_ = 0;

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  Initiate first measurement
  */
  if (!is_initialized_) {
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR ){
      float rho = meas_package.raw_measurements_[0];
      float phi = meas_package.raw_measurements_[1];
      float rhodot = meas_package.raw_measurements_[2];

      float px = rho*cos(phi);
      float py = rho*sin(phi);
      x_ << px, py, rhodot, phi, 0;
      use_radar_ = false;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER){
      x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0, 0, 0;
      use_laser_ = false;
    }
    time_us_ = meas_package.timestamp_;
    is_initialized_ = true;
    return;
  }

  double dt = (meas_package.timestamp_ - time_us_)/1000000.0;
  time_us_ = meas_package.timestamp_;
  // Predict State and Covariance //
  Prediction(dt);

  // Measurement update state and covariance //
  if(use_laser_){
    UpdateLidar(meas_package);
    use_laser_ = false;
    use_radar_ = true;
    return;
  }

  if(use_radar_){
    UpdateRadar(meas_package);
    use_radar_ = false;
    use_laser_ = true;
    return;
  }
//  if (meas_package.sensor_type_ == MeasurementPackage::RADAR ){
//    UpdateRadar(meas_package);
//  }else if (meas_package.sensor_type_ == MeasurementPackage::LASER){
//    UpdateLidar(meas_package);
//  }

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  lambda_ = 3 - n_aug_;
  // Augmented mean vector //
  VectorXd X_aug = VectorXd(n_aug_);
  X_aug.head(n_x_) = x_;
  X_aug(n_x_) = 0;
  X_aug(n_x_+1) = 0;

  // Augmented state covariance //
  MatrixXd P_aug(n_aug_, n_aug_);
  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  // Augmented square root matrix //
  MatrixXd A_aug = P_aug.llt().matrixL();

  // Augmented sigma point matrix //
  MatrixXd Xsig_aug(n_aug_, 2*n_aug_ + 1);
  Xsig_aug.col(0) = X_aug;
  for(int i=0; i<n_aug_;i++){
    Xsig_aug.col(i+1) = X_aug + sqrt(lambda_ + n_aug_)*A_aug.col(i);
    Xsig_aug.col(i+1+n_aug_) = X_aug - sqrt(lambda_ + n_aug_)*A_aug.col(i);
  }

  // Predict Sigma Points //
  for(int i=0; i<2*n_aug_+1;i++){
    double px = Xsig_aug(0,i);
    double py = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawdot = Xsig_aug(4,i);
    double mu_a = Xsig_aug(5,i);
    double mu_yaw = Xsig_aug(6,i);
    // Step //
    double px_step, py_step, v_step, yaw_step, yawdot_step;
    if(fabs(yawdot) > 0.001){
      px_step = v*(sin(yaw + yawdot*delta_t) - sin(yaw))/yawdot;
      py_step = v*(-cos(yaw + yawdot*delta_t) + cos(yaw))/yawdot;
    } else{
      px_step = v*cos(yaw)*delta_t;
      py_step = v*sin(yaw)*delta_t;
    }
    // v_step = 0, yaw_step = yawdot*delta_t, yawdot_step = 0; //

    Xsig_pred_(0,i) = px + px_step + 0.5*delta_t*delta_t*cos(yaw)*mu_a;
    Xsig_pred_(1,i) = py + py_step + 0.5*delta_t*delta_t*sin(yaw)*mu_a;
    Xsig_pred_(2,i) = v + delta_t*mu_a;
    Xsig_pred_(3,i) = yaw + yawdot*delta_t + 0.5*delta_t*delta_t*mu_yaw;
    Xsig_pred_(4,i) = yawdot + delta_t*mu_yaw;
  }

  // Weight vector //
  weights_(0) = lambda_/(lambda_+n_aug_);
  for(int i=1; i<2*n_aug_+1; i++){
    weights_(i) = 1/(2*(lambda_ + n_aug_));
  }
  // Predict State Mean //
  x_.fill(0.0);
  for(int i=0; i<2*n_aug_+1;i++){
    x_ = x_+ weights_(i)*Xsig_pred_.col(i);
  }
  // Predict State Covariance Matrix //
  P_.fill(0.0);
  for(int i=0; i<2*n_aug_+1;i++){
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
//    while (x_diff(3)>M_PI) x_diff(3) -= 2*M_PI;
//    while (x_diff(3)<-M_PI) x_diff(3) += 2*M_PI;
    x_diff(3) = atan2(sin(x_diff(3)), cos(x_diff(3)));
    P_ += weights_(i)*x_diff*x_diff.transpose();
  }
  }

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {

  MatrixXd H_ = MatrixXd(2,5);
  H_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0;
  //Measurement update //
  VectorXd y = meas_package.raw_measurements_ - H_*x_;

  // Lidar measurement noise //
  MatrixXd R_ = MatrixXd(2,2);
  R_ << std_laspx_*std_laspx_, 0,
          0, std_laspy_*std_laspy_;
  // measurement covariance //
  MatrixXd S = H_*P_*H_.transpose() + R_;
  // kalman gain //
  MatrixXd K_ = P_*H_.transpose()*S.inverse();
  // State update //
  x_ = x_ + K_*y;
  // Identity Matrix //
  MatrixXd I_ = MatrixXd::Identity(n_x_,n_x_);
  // State covariance update //
  P_ = (I_ - K_*H_)*P_;

  // NIS //
  NIS_laser_ = y.transpose()*S.inverse()*y;

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {

  // predict measurement sigma //
  MatrixXd Zsig_pred = MatrixXd(3,2*n_aug_+1);
  for( int i=0; i<2*n_aug_+1; i++){
    if(fabs(Xsig_pred_(0,i)) < 0.001 && fabs(Xsig_pred_(1,i)) < 0.001){
      Xsig_pred_(0,i) = 0.01;
      Xsig_pred_(1,i) = 0.01;
    }
    Zsig_pred(0,i) = sqrt(Xsig_pred_(0,i)*Xsig_pred_(0,i) + Xsig_pred_(1,i)*Xsig_pred_(1,i));
    Zsig_pred(1,i) = atan2(Xsig_pred_(1,i),Xsig_pred_(0,i));
    Zsig_pred(2,i) = (Xsig_pred_(0,i)*cos(Xsig_pred_(3,i))*Xsig_pred_(2,i) + Xsig_pred_(1,i)*sin(Xsig_pred_(3,i))*Xsig_pred_(2,i))/Zsig_pred(0,i);
  }

  // predicted measurement mean //
  VectorXd z_pred = VectorXd(3);
  z_pred.fill(0.0);
  for(int i=0; i<2*n_aug_+1;i++){
    z_pred += weights_(i)*Zsig_pred.col(i);
  }

  // Radar noise //
  MatrixXd R_ = MatrixXd(3,3);
  R_ << std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_,0,
          0, 0, std_radrd_*std_radrd_;

  // predicted measurement covariance and cross correlation matrix //
  MatrixXd S_ = MatrixXd(3,3);
  MatrixXd T_ = MatrixXd(n_x_,3);
  S_.fill(0.0);
  T_.fill(0.0);
  for(int i=0; i<2*n_aug_+1; i++){
    VectorXd z_diff = Zsig_pred.col(i) - z_pred;
//    while (z_diff(1) > M_PI) z_diff(1) -= 2*M_PI;
//    while (z_diff(1) < -M_PI) z_diff(1) += 2*M_PI;
    z_diff(1) = atan2(sin(z_diff(1)), cos(z_diff(1)));
    S_ += weights_(i)*z_diff*z_diff.transpose();

    VectorXd x_diff = Xsig_pred_.col(i) - x_;
//    while (x_diff(3)> M_PI) x_diff(3) -= 2*M_PI;
//    while (x_diff(3)<-M_PI) x_diff(3) += 2*M_PI;
    x_diff(3) = atan2(sin(x_diff(3)), cos(x_diff(3)));
    T_ += weights_(i)*x_diff*z_diff.transpose();
  }
  S_ = S_ + R_;

  // kalman gain //
  MatrixXd K_ = T_*S_.inverse();

  // update state //
  VectorXd z_diff = meas_package.raw_measurements_ - z_pred;
//  while (z_diff(1) > M_PI) z_diff(1) -= 2*M_PI;
//  while (z_diff(1) < -M_PI) z_diff(1) += 2*M_PI;
  z_diff(1) = atan2(sin(z_diff(1)), cos(z_diff(1)));
  x_ = x_ + K_*z_diff;

  // update covariance matrix //
  P_ = P_ - K_*S_*K_.transpose();

  // NIS //
  NIS_radar_ = z_diff.transpose()*S_.inverse()*z_diff;

}
