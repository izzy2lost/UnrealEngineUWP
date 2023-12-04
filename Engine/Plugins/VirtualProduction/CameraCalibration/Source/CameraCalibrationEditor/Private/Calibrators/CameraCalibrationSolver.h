// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Models/LensModel.h"
#include "Templates/SubclassOf.h"

#if WITH_OPENCV

#include "PreOpenCVHeaders.h"
#include <opencv2/core.hpp>
#include "PostOpenCVHeaders.h"

#endif	// WITH_OPENCV

#include "CameraCalibrationSolver.generated.h"

/** 
 * An array of 3D object points associated with a single calibration image.
 * Structure is needed because Blueprints and Python do not support arrays of arrays.
 */
USTRUCT(BlueprintType)
struct FObjectPoints
{
	GENERATED_BODY()

	/** 3D object points in an image */
	UPROPERTY(BlueprintReadWrite, Category = "Calibration")
	TArray<FVector> Points;
};

/** 
 * An array of 2D image points associated with a single calibration image 
 * Structure is needed because Blueprints and Python do not support arrays of arrays.
 */
USTRUCT(BlueprintType)
struct FImagePoints
{
	GENERATED_BODY()

	/** 2D image points in an image */
	UPROPERTY(BlueprintReadWrite, Category = "Calibration")
	TArray<FVector2D> Points;
};

/** 
 * Base lens distortion solver class that can be inherited from in Blueprints or Python 
 */
UCLASS(Blueprintable)
class ULensDistortionSolver : public UObject
{
	GENERATED_BODY()

public:
	/** Calibrate camera intrinsics and distortion from a set of input 3D-2D point correspondences and initial camera intrinsics guess. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Calibration")
	FDistortionCalibrationResult Solve(
		const TArray<FObjectPoints>& ObjectPointArray,
		const TArray<FImagePoints>& ImagePointArray,
		const FIntPoint ImageSize,
		const FVector2D& FocalLength,
		const FVector2D& ImageCenter) const;
};

/**
  * Flags used to modify the execution of the calibration solver
  */
enum class ECalibrationFlags : uint32
{
	None = 0,
	UseIntrinsicGuess = 1 << 0,  /** The solver will skip intrinsics initialization an use the input values of Focal Length and Image Center as the starting values for the optimization */
	UseExtrinsicGuess = 1 << 1,  /** The solver will skip extrinsics initialization an use the input Camera Poses as the starting value for the optimization */
	FixFocalLength = 1 << 2,     /** The solver will not optimize the focal length */
	FixPrincipalPoint = 1 << 3,  /** The solver will not optimize the principal point */
	FixExtrinsics = 1 << 4,      /** The solver will not optimize the camera extrinsics */
	FixZeroDistortion = 1 << 5   /** The solver will fix all distortion values at 0 */
};

/** 
  * Lens Distortion Solver class, supporting anamorphic and spherical models 
  * The implementation is largely based on the implementation of calibrateCamera from OpenCV: https://github.com/opencv/opencv
  */
class FCameraCalibrationSolver
{
public:
	/** 
	  * Calibrate camera intrinsics and distortion parameters using a set of input 3D and 2D point correspondences
	  * Returns the root mean reprojection error between the input image points and the projection of the input object points
	  */
	static double CalibrateCamera(
		const TSubclassOf<ULensModel> LensModel,
		const TArray<FObjectPoints>& InObjectPointsArray,
		const TArray<FImagePoints>& InImagePointsArray,
		const FIntPoint ImageSize,
		FVector2D& InOutFocalLength,
		FVector2D& InOutImageCenter,
		TArray<float>& OutDistCoeffs,
		TArray<FTransform>& InOutCameraPoses,
		double PixelAspect = 1.0,
		ECalibrationFlags SolverFlags = ECalibrationFlags::None);

	/** Optimize the input nodal offset transform by running a downhill solver that attempts to minimize the reprojection error of the input points */
	static double OptimizeNodalOffset(
		const TArray<TArray<FVector>>& InObjectPoints, 
		const TArray<TArray<FVector2f>>& InImagePoints, 
		const FVector2D& InFocalLength,
		const FVector2D& InImageCenter,
		const TArray<FTransform>& InCameraPoses,
		FTransform& InOutNodalOffset);

#if WITH_OPENCV
private:
	/** Initialize the camera matrix of intrinsic parameters using linear algebra techniques */
	static void InitCameraIntrinsics(
		const cv::Mat& ObjectPoints,
		const cv::Mat& ImagePoints,
		const cv::Mat& NumPoints,
		const cv::Size ImageSize,
		cv::Mat& CameraMatrix);

	/** Initialize the camera extrinsics (rotation and translation vectors) for each image using linear algebra techniques */
	static void InitCameraExtrinsics(
		const TSubclassOf<ULensModel> LensModel,
		const cv::Mat& ObjectPoints,
		const cv::Mat& ImagePoints,
		const cv::Mat& CameraMatrix,
		const cv::Mat& DistCoeffs,
		const cv::Size ImageSize,
		cv::Mat& Rotation,
		cv::Mat& Translation,
		const ECalibrationFlags SolverFlags);

	/** Project the input object points to 2D using the input camera intrinsics, extrinsics, and distortion parameters */
	static void ProjectPoints(
		const TSubclassOf<ULensModel> LensModel,
		const cv::Mat& ObjectPoints,
		const cv::Mat& Rotation,
		const cv::Mat& Translation,
		const cv::Mat& CameraMatrix,
		const cv::Mat& DistCoeffs,
		const cv::Size ImageSize,
		cv::Mat& ProjectedPoints);

	/** Project the input object points to 2D using the input camera intrinsics, extrinsics, and distortion parameters */
	static void ProjectPoints(
		const TSubclassOf<ULensModel> LensModel,
		const cv::Mat& ObjectPoints,
		const cv::Mat& Rotation,
		const cv::Mat& Translation,
		const cv::Mat& CameraMatrix,
		const cv::Mat& DistCoeffs,
		const cv::Size ImageSize,
		cv::Mat& ProjectedPoints,
		cv::Mat& Jacobian,
		ECalibrationFlags SolverFlags);

	/** Project the input object points to 2D using the input camera intrinsics, extrinsics, and anamorphic distortion parameters */
	static void ProjectPointsAnamorphic(
		const cv::Mat& ObjectPoints,
		const cv::Mat& Rotation,
		const cv::Mat& Translation,
		const cv::Mat& CameraMatrix,
		const cv::Mat& DistCoeffs,
		const cv::Size ImageSize,
		cv::Mat& ProjectedPoints,
		cv::Mat& Jacobian,
		ECalibrationFlags SolverFlags);

	/** Project the input object points to 2D using the input camera intrinsics, extrinsics, and spherical distortion parameters */
	static void ProjectPointsSpherical(
		const cv::Mat& ObjectPoints,
		const cv::Mat& Rotation,
		const cv::Mat& Translation,
		const cv::Mat& CameraMatrix,
		const cv::Mat& DistCoeffs,
		cv::Mat& ProjectedPoints,
		cv::Mat& Jacobian,
		ECalibrationFlags SolverFlags);

	/** Copy the input 3D and 2D points to OpenCV matrices for ease of use with the solver */
	static void GatherPoints(
		const TArray<FObjectPoints>& InObjectPointsArray,
		const TArray<FImagePoints>& InImagePointsArray,
		cv::Mat& ObjectPointsMat,
		cv::Mat& ImagePointsMat);

	/** Divide large jacobian matrix into smaller views for each of the parameter groups the solver will solve */
	static void SubdivideJacobian(
		const cv::Mat& Jacobian,
		cv::Mat& JacRotation,
		cv::Mat& JacTranslation,
		cv::Mat& JacFocalLength,
		cv::Mat& JacImageCenter,
		cv::Mat& JacDistortion,
		ECalibrationFlags SolverFlags);

#endif	// WITH_OPENCV
};

#if WITH_OPENCV

/** Levenberg–Marquardt solver for solving least-squares problems */
class FLevMarqSolver
{
public:
	/** Mode for copying the contents of symmetric matrix */
	enum class ESymmetryMode : uint8
	{
		LowerToUpper,
		UpperToLower
	};

	/** State used by the solver in its state machine */
	enum class ESolverState : uint8
	{
		Done = 0,
		Started = 1,
		ComputeJacobian = 2,
		CheckError = 3
	};

	FLevMarqSolver(int NumParamsToSolve, int NumErrors, int NumMaxIterations, ESymmetryMode SymmetryMode = ESymmetryMode::LowerToUpper);

	/** Update the solver with the input jacobian and error matrices */
	bool Update();

	/** Update the solver with the input pre-multiplied jacobian and error matrices */
	bool UpdateAlt();

private:
	/** Step the solver, modifying the values of each of the parameters */
	void Step();

	/** Create a submatrix from the source matrix using only the columns and rows specified */
	void SubMatrix(const cv::Mat& Src, cv::Mat& Dst, const cv::Mat& Cols, const cv::Mat& Rows);

public:
	/** Parameters solved by this solver */
	cv::Mat Params;

	/** Mask specifiying which parameters in the parameter list should not be modified */
	cv::Mat Mask;

	/** Current solver state */
	ESolverState State = ESolverState::Started;

	/** Average error for the current parameter set */
	double ErrorNorm = 0;

	/** Jacobian Matrix */
	cv::Mat Jacobian;

	/** Error Matrix */
	cv::Mat Error;

	/** Jacobian Transposed multiplied by the Jacobian */
	cv::Mat JacTJac;

	/** Jacobian Transposed multiplied by the Error */
	cv::Mat JacTErr;

private:
	/** Stores the previous values of each parameter */
	cv::Mat PreviousParams;

	/** Previous average error */
	double PreviousErrorNorm = DBL_MAX;

	/** Maximum number of iterations the solver will do */
	int MaxIterations = 0;

	/** Current number of iterations */
	int Iterations = 0;

	/** Mode to use when copying half of the symmetric matrix */
	ESymmetryMode SymmMode;

	int LambdaLog10 = -3;
};

class FOptimizeNodalOffsetSolver : public cv::MinProblemSolver::Function
{
public:

	//~ Begin cv::MinProblemSolver::Function interface
	int getDims() const;
	double calc(const double* x) const;
	//~ End cv::MinProblemSolver::Function interface

public:
	FVector2D FocalLength;
	FVector2D ImageCenter;

	TArray<FTransform> CameraPoses;
	TArray<TArray<FVector>> Points3d;
	TArray<TArray<FVector2f>> Points2d;
};

#endif // WITH_OPENCV
