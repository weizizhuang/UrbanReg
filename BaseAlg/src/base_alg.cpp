#include "base_alg.h"
#include <pcl/registration/icp.h>
#include <SymEigsSolver.h>

void BaseAlg::icp(const Eigen::MatrixXd& v_1, const Eigen::MatrixXd& v_2, Eigen::MatrixXd& aligned_v_2)
{
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_1(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_2(new pcl::PointCloud<pcl::PointXYZ>);

	cloud_1->points.resize(v_1.rows());
	for (int i = 0; i < v_1.rows(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			cloud_1->points[i].data[j] = static_cast<float>(v_1(i, j));
		}
		cloud_1->points[i].data[3] = 1.0f;
	}

	cloud_2->points.resize(v_2.rows());
	for (int i = 0; i < v_2.rows(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			cloud_2->points[i].data[j] = static_cast<float>(v_2(i, j));
		}
		cloud_2->points[i].data[3] = 1.0f;
	}

	pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
	icp.setInputCloud(cloud_2);
	icp.setInputTarget(cloud_1);

	icp.setMaxCorrespondenceDistance(0.1);
	icp.setMaximumIterations(50);
	icp.setTransformationEpsilon(1e-8);
	icp.setEuclideanFitnessEpsilon(1);

	pcl::PointCloud<pcl::PointXYZ> final;
	icp.align(final);
	std::cout << "has converged:" << icp.hasConverged() << " score: " <<
		icp.getFitnessScore() << std::endl;
	std::cout << icp.getFinalTransformation() << std::endl;

	std::cout << icp.getFinalTransformation() * icp.getFinalTransformation().transpose().eval() << std::endl;

	aligned_v_2.resize(final.points.size(), 3);
	for (int i = 0; i < final.points.size(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			aligned_v_2(i, j) = static_cast<double>(final.points[i].data[j]);
		}
	}
}

Eigen::Matrix4d BaseAlg::normalize(const Eigen::VectorXd& min_corner, const Eigen::VectorXd& max_corner, Eigen::MatrixXd& v)
{
	assert(min_corner.size() == v.cols());
	assert(max_corner.size() == v.cols());
	Eigen::VectorXd min_v = v.colwise().minCoeff();
	Eigen::VectorXd max_v = v.colwise().maxCoeff();
	Eigen::RowVectorXd bbox_center(min_v.rows());
	double max_range = std::numeric_limits<double>::min();
	int m_id = -1;
	for (int i = 0; i < v.cols(); i++)
	{
		double range = max_v[i] - min_v[i];
		if (range > max_range)
		{
			max_range = range;
			m_id = i;
		}
		bbox_center(i) = (min_v(i) + max_v(i)) * 0.5;
	}
	double scale_ratio = (max_corner[m_id] - min_corner[m_id]) / max_range;
	v = (v.rowwise() - bbox_center) * scale_ratio;
	Eigen::RowVectorXd trans = (max_corner + min_corner) * 0.5;
	v.rowwise() += trans;

	Eigen::Matrix4d trans_mat;
	trans_mat.setZero();
	for(int i = 0; i < 3; ++i)
	{
		trans_mat(i, i) = scale_ratio;
		trans_mat(i, 3) = trans(i) - scale_ratio * bbox_center(i);
	}
	trans_mat(3, 3) = 1;
	return trans_mat;
}

int BaseAlg::pca(const Eigen::MatrixXd& data_mat, int ev_num, Eigen::VectorXd& eigen_values, Eigen::MatrixXd& eigen_vectors)
{
	Eigen::RowVectorXd center = data_mat.colwise().mean();

	Eigen::MatrixXd feature_centered = data_mat.rowwise() - center;

	Eigen::MatrixXd cov = (feature_centered.adjoint() * feature_centered) / double(data_mat.rows() - 1);

	// Construct matrix operation object using the wrapper class DenseSymMatProd
	Spectra::DenseSymMatProd<double> op(cov);

	// Construct eigen solver object, requesting the largest ev_num eigenvalues
	Spectra::SymEigsSolver< double, Spectra::LARGEST_MAGN, Spectra::DenseSymMatProd<double> > eigs(&op, ev_num, ev_num * 2);

	// Initialize and compute
	eigs.init();
	int nconv = eigs.compute();

	// Retrieve results
	if (eigs.info() == Spectra::SUCCESSFUL)
	{
		eigen_values = eigs.eigenvalues();
		eigen_vectors = eigs.eigenvectors();
	}
	return eigs.info();
}