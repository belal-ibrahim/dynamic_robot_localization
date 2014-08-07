#pragma once

/**\file localization.hpp
 * \brief Description...
 *
 * @version 1.0
 * @author Carlos Miguel Correia da Costa
 */

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <includes>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#include <dynamic_robot_localization/localization/localization.h>
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </includes>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



namespace dynamic_robot_localization {
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <imports>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </imports>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// =============================================================================  <public-section>  ============================================================================
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <constructors-destructor>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
template<typename PointT>
Localization<PointT>::Localization() :
	save_reference_pointclouds_in_binary_format_(true),
	max_outliers_percentage_(0.6),
	publish_tf_map_odom_(false),
	add_odometry_displacement_(false),
	last_scan_time_(0),
	last_map_received_time_(0),
	reference_pointcloud_received_(false),
	reference_pointcloud_2d_(false),
	ignore_height_corrections_(false),
	last_accepted_pose_valid_(false),
	reference_pointcloud_(new pcl::PointCloud<PointT>()),
	reference_pointcloud_search_method_(new pcl::search::KdTree<PointT>()),
	outlier_percentage_(0.0) {}

template<typename PointT>
Localization<PointT>::~Localization() {}
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </constructors-destructor>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   <Localization-functions>   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
template<typename PointT>
void Localization<PointT>::setupConfigurationFromParameterServer(ros::NodeHandlePtr& node_handle, ros::NodeHandlePtr& private_node_handle, std::string configuration_namespace) {
	node_handle_ = node_handle;
	private_node_handle_ = private_node_handle;

	// general configurations
	setupGeneralConfigurations();
	setupSubcriptionTopicNames();
	setupPublishTopicNames();
	setupFrameIds();
	setupMessageManagement();

	// localization pipeline configurations
	setupReferencePointCloud();
	setupFiltersConfigurations();
	setupNormalEstimatorConfigurations();
	setupKeypointDetectors();
	setupPointCloudMatchersConfigurations();
	setupFeatureCloudMatchersConfigurations();
	setupTransformationValidatorsConfigurations();
	setupOutlierDetectorsConfigurations();

	pose_to_tf_publisher_.setupConfigurationFromParameterServer(node_handle, private_node_handle, "");
	if (publish_tf_map_odom_) {
		pose_to_tf_publisher_.publishInitialPoseFromParameterServer();
	}
}


template<typename PointT>
void Localization<PointT>::setupGeneralConfigurations() {
	private_node_handle_->param("transformation_validators/publish_tf_map_odom", publish_tf_map_odom_, false);
	private_node_handle_->param("transformation_validators/add_odometry_displacement", add_odometry_displacement_, false);
}


template<typename PointT>
void Localization<PointT>::setupSubcriptionTopicNames() {
	private_node_handle_->param("subscribe_topic_names/ambient_pointcloud_topic", ambient_pointcloud_topic_, std::string("ambient_pointcloud"));
	private_node_handle_->param("subscribe_topic_names/reference_costmap_topic", reference_costmap_topic_, std::string("/map"));
	private_node_handle_->param("subscribe_topic_names/reference_pointcloud_topic", reference_pointcloud_topic_, std::string(""));
}


template<typename PointT>
void Localization<PointT>::setupPublishTopicNames() {
	private_node_handle_->param("publish_topic_names/reference_pointcloud_publish_topic", reference_pointcloud_publish_topic_, std::string("reference_pointcloud"));
	private_node_handle_->param("publish_topic_names/aligned_pointcloud_publish_topic", aligned_pointcloud_publish_topic_, std::string("aligned_pointcloud"));
	private_node_handle_->param("publish_topic_names/pose_publish_topic", pose_publish_topic_, std::string("/initialpose"));
	private_node_handle_->param("publish_topic_names/localization_detailed_publish_topic", localization_detailed_publish_topic_, std::string("localization_detailed"));
	private_node_handle_->param("publish_topic_names/localization_diagnostics_publish_topic", localization_diagnostics_publish_topic_, std::string("diagnostics"));
	private_node_handle_->param("publish_topic_names/localization_times_publish_topic", localization_times_publish_topic_, std::string("localization_times"));
}


template<typename PointT>
void Localization<PointT>::setupFrameIds() {
	private_node_handle_->param("frame_ids/map_frame_id", map_frame_id_, std::string("map"));
	private_node_handle_->param("frame_ids/base_link_frame_id", base_link_frame_id_, std::string("base_footprint"));
	private_node_handle_->param("frame_ids/sensor_frame_id", sensor_frame_id_, std::string("hokuyo_tilt_laser_link"));
}


template<typename PointT>
void Localization<PointT>::setupMessageManagement() {
	double max_seconds_ambient_pointcloud_age;
	private_node_handle_->param("message_management/max_seconds_ambient_pointcloud_age", max_seconds_ambient_pointcloud_age, 0.5);
	max_seconds_ambient_pointcloud_age_.fromSec(max_seconds_ambient_pointcloud_age);

	double min_seconds_between_scan_registration;
	private_node_handle_->param("message_management/min_seconds_between_scan_registration", min_seconds_between_scan_registration, 0.05);
	min_seconds_between_scan_registration_.fromSec(min_seconds_between_scan_registration);

	double min_seconds_between_reference_pointcloud_update;
	private_node_handle_->param("message_management/min_seconds_between_reference_pointcloud_update", min_seconds_between_reference_pointcloud_update, 5.0);
	min_seconds_between_reference_pointcloud_update_.fromSec(min_seconds_between_reference_pointcloud_update);
}


template<typename PointT>
void Localization<PointT>::setupReferencePointCloud() {
	private_node_handle_->param("reference_pointclouds/reference_pointcloud_filename", reference_pointcloud_filename_, std::string(""));
	private_node_handle_->param("reference_pointclouds/reference_pointcloud_preprocessed_save_filename", reference_pointcloud_preprocessed_save_filename_, std::string(""));
	private_node_handle_->param("reference_pointclouds/reference_pointcloud_keypoints_filename", reference_pointcloud_keypoints_filename_, std::string(""));
	private_node_handle_->param("reference_pointclouds/reference_pointcloud_keypoints_save_filename", reference_pointcloud_keypoints_save_filename_, std::string(""));
	private_node_handle_->param("reference_pointclouds/save_reference_pointclouds_in_binary_format", save_reference_pointclouds_in_binary_format_, true);
}


template<typename PointT>
void Localization<PointT>::setupFiltersConfigurations() {
	ambient_cloud_filters_.clear();
	reference_cloud_filters_.clear();

	loadFiltersFromParameterServer(reference_cloud_filters_, "filters/reference_pointcloud/");
	loadFiltersFromParameterServer(ambient_cloud_filters_, "filters/ambient_pointcloud/");
}


template<typename PointT>
void Localization<PointT>::loadFiltersFromParameterServer(std::vector< typename CloudFilter<PointT>::Ptr >& filters_container, std::string configuration_namespace) {
	XmlRpc::XmlRpcValue filters;
	if (private_node_handle_->getParam(configuration_namespace, filters) && filters.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = filters.begin(); it != filters.end(); ++it) {
			std::string filter_name = it->first;
			typename CloudFilter<PointT>::Ptr cloud_filter;
			if (filter_name.find("voxel_grid") != std::string::npos) {
				cloud_filter.reset(new VoxelGrid<PointT>());
			} else if (filter_name.find("pass_through") != std::string::npos) {
				cloud_filter.reset(new PassThrough<PointT>());
			}

			if (cloud_filter) {
				cloud_filter->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + filter_name + "/");
				filters_container.push_back(cloud_filter);
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupNormalEstimatorConfigurations() {
	loadNormalEstimatorFromParameterServer(reference_cloud_normal_estimator_, "normal_estimators/reference_pointcloud/");
	loadNormalEstimatorFromParameterServer(ambient_cloud_normal_estimator_, "normal_estimators/ambient_pointcloud/");
}


template<typename PointT>
void Localization<PointT>::loadNormalEstimatorFromParameterServer(typename NormalEstimator<PointT>::Ptr& normal_estimator, std::string configuration_namespace) {
	normal_estimator.reset();
	XmlRpc::XmlRpcValue normal_estimators;
	if (private_node_handle_->getParam(configuration_namespace, normal_estimators) && normal_estimators.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = normal_estimators.begin(); it != normal_estimators.end(); ++it) {
			std::string estimator_name = it->first;
			if (estimator_name.find("normal_estimation_omp") != std::string::npos) {
				normal_estimator = typename NormalEstimator<PointT>::Ptr(new NormalEstimationOMP<PointT>());
			} else if (estimator_name.find("moving_least_squares") != std::string::npos) {
				normal_estimator = typename NormalEstimator<PointT>::Ptr(new MovingLeastSquares<PointT>());
			}

			if (normal_estimator) {
				normal_estimator->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + estimator_name + "/");
				return;
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupKeypointDetectors() {
	reference_cloud_keypoint_detectors_.clear();
	ambient_cloud_keypoint_detectors_.clear();

	loadKeypointDetectorsFromParameterServer(reference_cloud_keypoint_detectors_, "cloud_matchers/feature_matcher/keypoint_detectors/reference_pointcloud/");
	loadKeypointDetectorsFromParameterServer(ambient_cloud_keypoint_detectors_, "cloud_matchers/feature_matcher/keypoint_detectors/ambient_pointcloud/");
}


template<typename PointT>
void Localization<PointT>::loadKeypointDetectorsFromParameterServer(std::vector<typename KeypointDetector<PointT>::Ptr >& keypoint_detectors, std::string configuration_namespace) {
	XmlRpc::XmlRpcValue detectors;
	if (private_node_handle_->getParam(configuration_namespace, detectors) && detectors.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = detectors.begin(); it != detectors.end(); ++it) {
			std::string detector_name = it->first;
			typename KeypointDetector<PointT>::Ptr keypoint_detector;
			if (detector_name.find("intrinsic_shape_signature_3d") != std::string::npos) {
				keypoint_detector.reset(new IntrinsicShapeSignature3D<PointT>());
			}

			if (keypoint_detector) {
				keypoint_detector->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + detector_name + "/");
				keypoint_detectors.push_back(keypoint_detector);
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupPointCloudMatchersConfigurations() {
	private_node_handle_->param("cloud_matchers/ignore_height_corrections", ignore_height_corrections_, false);
	pointcloud_matchers_.clear();

	std::string configuration_namespace = "cloud_matchers/point_matchers/";
	XmlRpc::XmlRpcValue matchers;
	if (private_node_handle_->getParam(configuration_namespace, matchers) && matchers.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = matchers.begin(); it != matchers.end(); ++it) {
			std::string matcher_name = it->first;
			typename CloudMatcher<PointT>::Ptr cloud_matcher;
			if (matcher_name.find("iterative_closest_point_with_normals") != std::string::npos) {
				cloud_matcher.reset(new IterativeClosestPointWithNormals<PointT>());
			} else if (matcher_name.find("iterative_closest_point") != std::string::npos) {
				cloud_matcher.reset(new IterativeClosestPoint<PointT>());
			}

			if (cloud_matcher) {
				cloud_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + matcher_name + "/");
				pointcloud_matchers_.push_back(cloud_matcher);
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupFeatureCloudMatchersConfigurations() {
	featurecloud_matchers_.clear();

	double pose_tracking_timeout;
	private_node_handle_->param("cloud_matchers/pose_tracking_timeout", pose_tracking_timeout, 2.0);
	pose_tracking_timeout_.fromSec(pose_tracking_timeout);

	std::string keypoint_descriptor_configuration_namespace = "cloud_matchers/feature_matchers/keypoint_descriptors/";
	XmlRpc::XmlRpcValue keypoint_descriptors;
	if (private_node_handle_->getParam(keypoint_descriptor_configuration_namespace, keypoint_descriptors) && keypoint_descriptors.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = keypoint_descriptors.begin(); it != keypoint_descriptors.end(); ++it) {
			std::string descriptor_name = it->first;
			if (descriptor_name.find("fpfh") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::FPFHSignature33>::Ptr keypoint_descriptor(new FPFH<PointT, pcl::FPFHSignature33>());
				loadKeypointMatcherFromParameterServer<pcl::FPFHSignature33>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			} else if (descriptor_name.find("pfh") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::PFHSignature125>::Ptr keypoint_descriptor(new PFH<PointT, pcl::PFHSignature125>());
				loadKeypointMatcherFromParameterServer<pcl::PFHSignature125>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			} else if (descriptor_name.find("shot") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::SHOT352>::Ptr keypoint_descriptor(new SHOT<PointT, pcl::SHOT352>());
				loadKeypointMatcherFromParameterServer<pcl::SHOT352>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			} else if (descriptor_name.find("shape_context_3d") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::ShapeContext1980>::Ptr keypoint_descriptor(new ShapeContext3D<PointT, pcl::ShapeContext1980>());
				loadKeypointMatcherFromParameterServer<pcl::ShapeContext1980>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			} else if (descriptor_name.find("unique_shape_context") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::ShapeContext1980>::Ptr keypoint_descriptor(new UniqueShapeContext<PointT, pcl::ShapeContext1980>());
				loadKeypointMatcherFromParameterServer<pcl::ShapeContext1980>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			}/* else if (descriptor_name.find("spin_image") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::Histogram<153> >::Ptr keypoint_descriptor(new SpinImage< PointT, pcl::Histogram<153> >());
				loadKeypointMatcherFromParameterServer< pcl::Histogram<153> >(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			}*/ else if (descriptor_name.find("esf") != std::string::npos) {
				typename KeypointDescriptor<PointT, pcl::ESFSignature640>::Ptr keypoint_descriptor(new ESF<PointT, pcl::ESFSignature640>());
				loadKeypointMatcherFromParameterServer<pcl::ESFSignature640>(keypoint_descriptor, keypoint_descriptor_configuration_namespace + descriptor_name + "/");
				return;
			}
		}
	}
}


template<typename PointT>
template<typename DescriptorT>
void Localization<PointT>::loadKeypointMatcherFromParameterServer(typename KeypointDescriptor<PointT, DescriptorT>::Ptr& keypoint_descriptor, std::string keypoint_descriptor_configuration_namespace) {
	keypoint_descriptor->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, keypoint_descriptor_configuration_namespace);

	std::string configuration_namespace = "cloud_matchers/feature_matchers/matchers/";
	XmlRpc::XmlRpcValue keypoint_descriptors;
	if (private_node_handle_->getParam(configuration_namespace, keypoint_descriptors) && keypoint_descriptors.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = keypoint_descriptors.begin(); it != keypoint_descriptors.end(); ++it) {
			std::string matcher_name = it->first;
			if (matcher_name.find("sample_consensus_initial_alignment_prerejective") != std::string::npos) {
				typename FeatureMatcher<PointT, DescriptorT>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignmentPrerejective<PointT, DescriptorT>());
				initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
				initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + matcher_name + "/");
				featurecloud_matchers_.push_back(initial_aligment_matcher);
			} else if (matcher_name.find("sample_consensus_initial_alignment") != std::string::npos) {
				typename FeatureMatcher<PointT, DescriptorT>::Ptr initial_aligment_matcher(new SampleConsensusInitialAlignment<PointT, DescriptorT>());
				initial_aligment_matcher->setKeypointDescriptor(keypoint_descriptor);
				initial_aligment_matcher->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + matcher_name + "/");
				featurecloud_matchers_.push_back(initial_aligment_matcher);
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupTransformationValidatorsConfigurations() {
	std::string configuration_namespace = "transformation_validators/";

	transformation_validators_.clear();
	XmlRpc::XmlRpcValue transformation_validators;
	if (private_node_handle_->getParam(configuration_namespace, transformation_validators) && transformation_validators.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = transformation_validators.begin(); it != transformation_validators.end(); ++it) {
			std::string validator_name = it->first;
			TransformationValidator::Ptr transformation_validator;
			if (validator_name.find("euclidean_transformation_validator") != std::string::npos) {
				transformation_validator.reset(new EuclideanTransformationValidator());
			}

			if (transformation_validator) {
				transformation_validator->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + validator_name + "/");
				transformation_validators_.push_back(transformation_validator);
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::setupOutlierDetectorsConfigurations() {
	outlier_detectors_.clear();

	std::string configuration_namespace = "outlier_detectors/";
	XmlRpc::XmlRpcValue outlier_detectors;
	if (private_node_handle_->getParam(configuration_namespace, outlier_detectors) && outlier_detectors.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
		for (XmlRpc::XmlRpcValue::iterator it = outlier_detectors.begin(); it != outlier_detectors.end(); ++it) {
			std::string detector_name = it->first;
			typename OutlierDetector<PointT>::Ptr outlier_detector;
			if (detector_name.find("euclidean_outlier_detector") != std::string::npos) {
				outlier_detector.reset(new EuclideanOutlierDetector<PointT>());
			}

			if (outlier_detector) {
				outlier_detector->setupConfigurationFromParameterServer(node_handle_, private_node_handle_, configuration_namespace + detector_name + "/");
				outlier_detectors_.push_back(outlier_detector);
			}
		}
	}
}


template<typename PointT>
bool Localization<PointT>::loadReferencePointCloudFromFile(const std::string& reference_pointcloud_filename) {
	if (pcl::io::loadPCDFile<PointT>(reference_pointcloud_filename, *reference_pointcloud_) == 0) {
		if (!reference_pointcloud_->points.empty()) {
			ROS_DEBUG_STREAM("Loaded reference point cloud from file " << reference_pointcloud_filename << " with " << reference_pointcloud_->points.size() << " points");
			reference_pointcloud_->header.frame_id = map_frame_id_;

			last_map_received_time_ = ros::Time::now();
			if (updateLocalizationPipelineWithNewReferenceCloud()) {
				reference_pointcloud_2d_ = false;
				return true;
			}
		}
	}

	return false;
}


template<typename PointT>
void Localization<PointT>::loadReferencePointCloudFromROSPointCloud(const sensor_msgs::PointCloud2ConstPtr& reference_pointcloud_msg) {
	if (!reference_pointcloud_received_ || (ros::Time::now() - last_map_received_time_) > min_seconds_between_reference_pointcloud_update_) {
		last_map_received_time_ = ros::Time::now();

		if (reference_pointcloud_msg->width > 0 && reference_pointcloud_msg->data.size() > 0 && reference_pointcloud_msg->fields.size() >= 3) {
			pcl::fromROSMsg(*reference_pointcloud_msg, *reference_pointcloud_);
			if (!reference_pointcloud_->points.empty()) {
				if (updateLocalizationPipelineWithNewReferenceCloud()) {
					reference_pointcloud_2d_ = false;
					ROS_DEBUG_STREAM("Loaded reference point cloud from cloud topic " << reference_pointcloud_topic_ << " with " << reference_pointcloud_->points.size() << " points");
				}
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::loadReferencePointCloudFromROSOccupancyGrid(const nav_msgs::OccupancyGridConstPtr& occupancy_grid_msg) {
	if (!reference_pointcloud_received_ || (ros::Time::now() - last_map_received_time_) > min_seconds_between_reference_pointcloud_update_) {
		last_map_received_time_ = ros::Time::now();

		if (pointcloud_conversions::fromROSMsg(*occupancy_grid_msg, *reference_pointcloud_)) {
			if (!reference_pointcloud_->points.empty()) {
				if (updateLocalizationPipelineWithNewReferenceCloud()) {
					reference_pointcloud_2d_ = true;
					ROS_DEBUG_STREAM("Loaded reference point cloud from costmap topic " << reference_costmap_topic_ << " with " << reference_pointcloud_->points.size() << " points");
				}
			}
		}
	}
}


template<typename PointT>
void Localization<PointT>::publishReferencePointCloud() {
	if (!reference_pointcloud_publisher_.getTopic().empty()) {
		sensor_msgs::PointCloud2Ptr reference_pointcloud(new sensor_msgs::PointCloud2());
		pcl::toROSMsg(*reference_pointcloud_, *reference_pointcloud);
		reference_pointcloud->header.frame_id = map_frame_id_;
		reference_pointcloud->header.stamp = ros::Time::now();
		reference_pointcloud_publisher_.publish(reference_pointcloud);
	}
}


template<typename PointT>
bool Localization<PointT>::updateLocalizationPipelineWithNewReferenceCloud() {
	localization_diagnostics_msg_.number_points_reference_pointcloud = reference_pointcloud_->points.size();

	if (!applyFilters(reference_cloud_filters_, reference_pointcloud_)) { return false; }
	localization_diagnostics_msg_.number_points_reference_pointcloud_after_filtering = reference_pointcloud_->points.size();

	reference_pointcloud_search_method_->setInputCloud(reference_pointcloud_);
	if (reference_cloud_normal_estimator_) {
		if (!applyNormalEstimation(reference_cloud_normal_estimator_, reference_pointcloud_, reference_pointcloud_search_method_)) { return false; }
		reference_pointcloud_search_method_->setInputCloud(reference_pointcloud_); // update kdtree
	}

	if (!reference_pointcloud_->points.empty()) {
		reference_pointcloud_received_ = true;

		if (!reference_pointcloud_preprocessed_save_filename_.empty()) {
			ROS_DEBUG_STREAM("Saving reference pointcloud preprocessed with " << reference_pointcloud_->points.size() << " points to file " << reference_pointcloud_preprocessed_save_filename_);
			pcl::io::savePCDFile<PointT>(reference_pointcloud_preprocessed_save_filename_, *reference_pointcloud_, save_reference_pointclouds_in_binary_format_);
		}

		typename pcl::PointCloud<PointT>::Ptr reference_pointcloud_keypoints(new pcl::PointCloud<PointT>());

		if (!reference_cloud_keypoint_detectors_.empty()) {
			if (reference_pointcloud_keypoints_filename_.empty() || pcl::io::loadPCDFile<PointT>(reference_pointcloud_keypoints_filename_, *reference_pointcloud_keypoints) != 0) {
				if (!applyKeypointDetection(reference_cloud_keypoint_detectors_, reference_pointcloud_, reference_pointcloud_search_method_, reference_pointcloud_keypoints)) { return false; }

				if (!reference_pointcloud_keypoints_save_filename_.empty()) {
					ROS_DEBUG_STREAM("Saving reference pointcloud keypoints with " << reference_pointcloud_keypoints->points.size() << " points to file " << reference_pointcloud_keypoints_save_filename_);
					pcl::io::savePCDFile<PointT>(reference_pointcloud_keypoints_save_filename_, *reference_pointcloud_keypoints, save_reference_pointclouds_in_binary_format_);
				}
			} else {
				ROS_DEBUG_STREAM("Loaded " << reference_pointcloud_keypoints->points.size() << " keypoints from file " << reference_pointcloud_keypoints_filename_);
			}
		}
		localization_diagnostics_msg_.number_keypoints_reference_pointcloud = reference_pointcloud_keypoints->points.size();


		for (size_t i = 0; i < pointcloud_matchers_.size(); ++i) {
			pointcloud_matchers_[i]->setupReferenceCloud(reference_pointcloud_, reference_pointcloud_keypoints, reference_pointcloud_search_method_);
		}

		publishReferencePointCloud();
		return true;
	}

	return false;
}


template<typename PointT>
void Localization<PointT>::startLocalization() {
	// publishers
	if (!reference_pointcloud_publish_topic_.empty()) reference_pointcloud_publisher_ = node_handle_->advertise<sensor_msgs::PointCloud2>(reference_pointcloud_publish_topic_, 2, true);
	if (!aligned_pointcloud_publish_topic_.empty()) aligned_pointcloud_publisher_ = node_handle_->advertise<sensor_msgs::PointCloud2>(aligned_pointcloud_publish_topic_, 5, true);
	if (!pose_publish_topic_.empty()) pose_publisher_ = node_handle_->advertise<geometry_msgs::PoseWithCovarianceStamped>(pose_publish_topic_, 10, true);
	if (!localization_detailed_publish_topic_.empty()) localization_detailed_publisher_ = node_handle_->advertise<dynamic_robot_localization::LocalizationDetailed>(localization_detailed_publish_topic_, 10, true);
	if (!localization_diagnostics_publish_topic_.empty()) localization_diagnostics_publisher_ = node_handle_->advertise<dynamic_robot_localization::LocalizationDiagnostics>(localization_diagnostics_publish_topic_, 10, true);
	if (!localization_times_publish_topic_.empty()) localization_times_publisher_ = node_handle_->advertise<dynamic_robot_localization::LocalizationTimes>(localization_times_publish_topic_, 10, true);


	// subscribers
	ambient_pointcloud_subscriber_ = node_handle_->subscribe(ambient_pointcloud_topic_, 2, &dynamic_robot_localization::Localization<PointT>::processAmbientPointCloud, this);

	if (reference_pointcloud_filename_.empty()) {
		if (!reference_pointcloud_topic_.empty()) {
			reference_pointcloud_subscriber_ = node_handle_->subscribe(reference_pointcloud_topic_, 1, &dynamic_robot_localization::Localization<PointT>::loadReferencePointCloudFromROSPointCloud, this);
		} else {
			if (!reference_costmap_topic_.empty())
				costmap_subscriber_ = node_handle_->subscribe(reference_costmap_topic_, 1, &dynamic_robot_localization::Localization<PointT>::loadReferencePointCloudFromROSOccupancyGrid, this);
		}
	} else {
		loadReferencePointCloudFromFile(reference_pointcloud_filename_);
	}


	if (publish_tf_map_odom_) {
		ros::Rate publish_rate(pose_to_tf_publisher_.getPublishRate());
		while (ros::ok()) {
			pose_to_tf_publisher_.publishTFMapToOdom();
			publish_rate.sleep();
			ros::spinOnce();
		}
	} else {
		ros::spin();
	}
}


template<typename PointT>
void Localization<PointT>::processAmbientPointCloud(const sensor_msgs::PointCloud2ConstPtr& ambient_cloud_msg) {
	PerformanceTimer performance_timer;
	performance_timer.start();
	localization_times_msg_ = LocalizationTimes();

	ros::Duration scan_age = ros::Time::now() - ambient_cloud_msg->header.stamp;
	ros::Duration elapsed_time_since_last_scan = ros::Time::now() - last_scan_time_;

	ROS_DEBUG_STREAM("Received pointcloud with " << (ambient_cloud_msg->width * ambient_cloud_msg->height) << " points and with time stamp " << ambient_cloud_msg->header.stamp);

	if (reference_pointcloud_received_
			&& ambient_cloud_msg->data.size() > 0
			&& elapsed_time_since_last_scan > min_seconds_between_scan_registration_
			&& scan_age < max_seconds_ambient_pointcloud_age_) {

		last_scan_time_ = ros::Time::now();

		tf2::Transform pose_tf_initial_guess;
		if (!pose_to_tf_publisher_.getTfCollector().lookForTransform(pose_tf_initial_guess, ambient_cloud_msg->header.frame_id, base_link_frame_id_, ambient_cloud_msg->header.stamp)) {
			ROS_DEBUG_STREAM("Dropping pointcloud because tf between " << ambient_cloud_msg->header.frame_id << " and " << base_link_frame_id_ << " isn't available");
			return;
		}

		typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud(new pcl::PointCloud<PointT>());
		pcl::fromROSMsg(*ambient_cloud_msg, *ambient_pointcloud);
		if (reference_pointcloud_2d_) {
			resetPointCloudHeight(*ambient_pointcloud);
		}

		localization_diagnostics_msg_.number_points_ambient_pointcloud = ambient_pointcloud->points.size();

		// >>>>> localization pipeline <<<<<
		tf2::Transform pose_tf_corrected;
		if (updateLocalizationWithAmbientPointCloud(ambient_pointcloud, pose_tf_initial_guess, pose_tf_corrected)) {
			geometry_msgs::PoseWithCovarianceStampedPtr pose_corrected_msg(new geometry_msgs::PoseWithCovarianceStamped());
			pose_corrected_msg->header.frame_id = ambient_cloud_msg->header.frame_id;

			if (ignore_height_corrections_) {
				pose_tf_corrected.getOrigin().setZ(pose_tf_initial_guess.getOrigin().getZ());
			}

			if (add_odometry_displacement_) {
				pose_corrected_msg->header.stamp = ros::Time::now();
				pose_to_tf_publisher_.addOdometryDisplacementToTransform(pose_tf_corrected, ambient_cloud_msg->header.stamp, pose_corrected_msg->header.stamp);
			} else {
				pose_corrected_msg->header.stamp = ambient_cloud_msg->header.stamp;
			}

			if (publish_tf_map_odom_)
				pose_to_tf_publisher_.publishTFMapToOdom(pose_tf_corrected, ambient_cloud_msg->header.stamp);

			laserscan_to_pointcloud::tf_rosmsg_eigen_conversions::transformTF2ToMsg(pose_tf_corrected, pose_corrected_msg->pose.pose);
			// todo: fill covariance
			// pose->pose.covariance

			if (!pose_publisher_.getTopic().empty()) {
				pose_publisher_.publish(pose_corrected_msg);
			}


			if (!localization_detailed_publisher_.getTopic().empty()) {
				tf2::Matrix3x3 pose_initial_guess_matrix(tf2::Quaternion(pose_tf_initial_guess.getRotation().getX(), pose_tf_initial_guess.getRotation().getY(), pose_tf_initial_guess.getRotation().getZ(), pose_tf_initial_guess.getRotation().getW()));
				tf2Scalar roll_initial_guess, pitch_initial_guess, yaw_initial_guess;
				pose_initial_guess_matrix.getRPY(roll_initial_guess, pitch_initial_guess, yaw_initial_guess);

				tf2::Matrix3x3 pose_corrected_matrix(tf2::Quaternion(pose_tf_corrected.getRotation().getX(), pose_tf_corrected.getRotation().getY(), pose_tf_corrected.getRotation().getZ(), pose_tf_corrected.getRotation().getW()));
				tf2Scalar roll_corrected, pitch_corrected, yaw_corrected;
				pose_corrected_matrix.getRPY(roll_corrected, pitch_corrected, yaw_corrected);

				LocalizationDetailed localization_detailed_msg;
				localization_detailed_msg.header.frame_id = ambient_cloud_msg->header.frame_id;
				localization_detailed_msg.header.stamp = ambient_cloud_msg->header.stamp;
				localization_detailed_msg.pose = pose_corrected_msg->pose;
				localization_detailed_msg.pose_corrections.position.x = (pose_tf_corrected.getOrigin().getX() - pose_tf_initial_guess.getOrigin().getX()) * 1000.0; // mm
				localization_detailed_msg.pose_corrections.position.y = (pose_tf_corrected.getOrigin().getY() - pose_tf_initial_guess.getOrigin().getY()) * 1000.0; // mm
				localization_detailed_msg.pose_corrections.position.z = (pose_tf_corrected.getOrigin().getZ() - pose_tf_initial_guess.getOrigin().getZ()) * 1000.0; // mm
				localization_detailed_msg.pose_corrections.orientation.x = angles::to_degrees(roll_corrected - roll_initial_guess);
				localization_detailed_msg.pose_corrections.orientation.y = angles::to_degrees(pitch_corrected - pitch_initial_guess);
				localization_detailed_msg.pose_corrections.orientation.z = angles::to_degrees(yaw_corrected - yaw_initial_guess);
				localization_detailed_msg.pose_corrections.orientation.w = localization_detailed_msg.pose_corrections.orientation.x + localization_detailed_msg.pose_corrections.orientation.y + localization_detailed_msg.pose_corrections.orientation.z;
				localization_detailed_msg.outlier_percentage = outlier_percentage_;
				localization_detailed_msg.aligment_fitness = pointcloud_matchers_.back()->getCloudMatcher()->getFitnessScore();
				localization_detailed_publisher_.publish(localization_detailed_msg);
			}

			if (!localization_times_publisher_.getTopic().empty()) {
				localization_times_msg_.header.frame_id = ambient_cloud_msg->header.frame_id;
				localization_times_msg_.header.stamp = ambient_cloud_msg->header.stamp;
				localization_times_msg_.global_time = performance_timer.getElapsedTimeInMilliSec();
				localization_times_publisher_.publish(localization_times_msg_);
			}

			if (!localization_diagnostics_publisher_.getTopic().empty()) {
				localization_diagnostics_msg_.header.frame_id = ambient_cloud_msg->header.frame_id;
				localization_diagnostics_msg_.header.stamp = ambient_cloud_msg->header.stamp;
				localization_diagnostics_publisher_.publish(localization_diagnostics_msg_);
			}

			if (!aligned_pointcloud_publisher_.getTopic().empty()) {
				sensor_msgs::PointCloud2Ptr aligned_pointcloud_msg(new sensor_msgs::PointCloud2());
				pcl::toROSMsg(*ambient_pointcloud, *aligned_pointcloud_msg);
				aligned_pointcloud_publisher_.publish(aligned_pointcloud_msg);
			}
		} else {
			ROS_DEBUG_STREAM("Discarded cloud because localization couldn't be calculated");
		}
	} else {
		if (!reference_pointcloud_received_) {
			ROS_DEBUG_STREAM("Discarded cloud because there is no reference cloud to compare to");
		} else {
			ROS_DEBUG_STREAM("Discarded cloud with age " << scan_age.toSec());
		}
	}
}


template<typename PointT>
void Localization<PointT>::resetPointCloudHeight(pcl::PointCloud<PointT>& pointcloud, float height) {
	for (size_t i = 0; i < pointcloud.points.size(); ++i) {
		pointcloud.points[i].z = height;
	}
}


template<typename PointT>
bool Localization<PointT>::applyFilters(std::vector< typename CloudFilter<PointT>::Ptr >& cloud_filters, typename pcl::PointCloud<PointT>::Ptr& pointcloud) {
	for (size_t i = 0; i < cloud_filters.size(); ++i) {
		typename pcl::PointCloud<PointT>::Ptr filtered_ambient_pointcloud(new pcl::PointCloud<PointT>());
		cloud_filters[i]->filter(pointcloud, filtered_ambient_pointcloud);
		pointcloud = filtered_ambient_pointcloud; // switch pointers
	}

	return !pointcloud->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyNormalEstimation(typename NormalEstimator<PointT>::Ptr& normal_estimator, typename pcl::PointCloud<PointT>::Ptr& pointcloud, typename pcl::search::KdTree<PointT>::Ptr& surface_search_method) {
	if (!normal_estimator) return false;

	tf2::Transform sensor_pose_tf_guess;
	if (!pose_to_tf_publisher_.getTfCollector().lookForTransform(sensor_pose_tf_guess, pointcloud->header.frame_id, sensor_frame_id_, pcl_conversions::fromPCL(pointcloud->header).stamp)) {
		sensor_pose_tf_guess.setIdentity();
	}/* else {
		pointcloud->sensor_origin_(0) = sensor_pose_tf_guess.getOrigin().getX();
		pointcloud->sensor_origin_(1) = sensor_pose_tf_guess.getOrigin().getY();
		pointcloud->sensor_origin_(2) = sensor_pose_tf_guess.getOrigin().getZ();
	}*/
	typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_with_normals(new pcl::PointCloud<PointT>());
	normal_estimator->estimateNormals(pointcloud, pointcloud, surface_search_method, sensor_pose_tf_guess, ambient_pointcloud_with_normals);
	pointcloud = ambient_pointcloud_with_normals; // switch pointers

	return !pointcloud->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyKeypointDetection(std::vector< typename KeypointDetector<PointT>::Ptr >& keypoint_detectors, typename pcl::PointCloud<PointT>::Ptr& pointcloud, typename pcl::search::KdTree<PointT>::Ptr& surface_search_method, typename pcl::PointCloud<PointT>::Ptr& keypoints) {
	keypoints->points.clear();
	for (size_t i = 0; i < keypoint_detectors.size(); ++i) {
		if (i == 0) {
			keypoint_detectors[i]->findKeypoints(pointcloud, keypoints, pointcloud, surface_search_method);
		} else {
			typename pcl::PointCloud<PointT>::Ptr keypoints_temp(new pcl::PointCloud<PointT>());
			keypoint_detectors[i]->findKeypoints(pointcloud, keypoints_temp, pointcloud, surface_search_method);
			*keypoints += *keypoints_temp;
		}
	}

	return !keypoints->points.empty();
}


template<typename PointT>
bool Localization<PointT>::applyCloudRegistration(std::vector< typename CloudMatcher<PointT>::Ptr >& matchers, typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud,
		typename pcl::search::KdTree<PointT>::Ptr& surface_search_method,
		typename pcl::PointCloud<PointT>::Ptr& pointcloud_keypoints,
		tf2::Transform& pointcloud_pose_in_out) {

	if (ambient_pointcloud->points.empty()) { return false; }

	bool registration_successful = false;
	for (size_t i = 0; i < matchers.size(); ++i) {
		typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_aligned(new pcl::PointCloud<PointT>());
		if (matchers[i]->registerCloud(ambient_pointcloud, surface_search_method, pointcloud_keypoints, pointcloud_pose_in_out, ambient_pointcloud_aligned, false)) {
			registration_successful = true;
			ambient_pointcloud = ambient_pointcloud_aligned; // switch pointers
		}
	}

	return registration_successful;
}


template<typename PointT>
double Localization<PointT>::applyOutlierDetection(typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud) {
	if (ambient_pointcloud->points.empty()) { return 0.0; }

	size_t number_outliers = 0;
	for (size_t i = 0; i < outlier_detectors_.size(); ++i) {
		sensor_msgs::PointCloud2Ptr outliers = outlier_detectors_[i]->processAndPublishOutliers(reference_pointcloud_search_method_, *ambient_pointcloud);
		number_outliers += ((size_t) ((outliers->width * outliers->height)));
	}

	return (double)number_outliers / (double) (ambient_pointcloud->points.size());
}


template<typename PointT>
bool Localization<PointT>::applyTransformationValidators(const tf2::Transform& pointcloud_pose_initial_guess, tf2::Transform& pointcloud_pose_corrected_in_out, double max_outlier_percentage) {
	for (size_t i = 0; i < transformation_validators_.size(); ++i) {
		if (!transformation_validators_[i]->validateNewLocalizationPose((last_accepted_pose_valid_ && (ros::Time::now() - last_accepted_pose_time_ < pose_tracking_timeout_)) ? last_accepted_pose_ : pointcloud_pose_initial_guess, pointcloud_pose_initial_guess, pointcloud_pose_corrected_in_out,
				pointcloud_matchers_.back()->getCloudMatcher()->getFitnessScore(), max_outlier_percentage)) {
			return false;
		}
	}

	return true;
}


template<typename PointT>
bool Localization<PointT>::updateLocalizationWithAmbientPointCloud(typename pcl::PointCloud<PointT>::Ptr& ambient_pointcloud, const tf2::Transform& pointcloud_pose_initial_guess, tf2::Transform& pointcloud_pose_corrected_out) {
	pointcloud_pose_corrected_out = pointcloud_pose_initial_guess;
	if (ambient_pointcloud->points.empty()) {
		return false;
	}

	PerformanceTimer performance_timer;
	performance_timer.start();
	// ==============================================================  filters
	if (!applyFilters(ambient_cloud_filters_, ambient_pointcloud)) { return false; }
	localization_diagnostics_msg_.number_points_ambient_pointcloud_after_filtering = ambient_pointcloud->points.size();
	localization_times_msg_.filtering_time = performance_timer.getElapsedTimeInMilliSec();


	// ==============================================================  normal estimation
	performance_timer.restart();
	typename pcl::search::KdTree<PointT>::Ptr ambient_search_method(new pcl::search::KdTree<PointT>());
	ambient_search_method->setInputCloud(ambient_pointcloud);
	if (ambient_cloud_normal_estimator_) {
		if (!applyNormalEstimation(ambient_cloud_normal_estimator_, ambient_pointcloud, ambient_search_method)) { return false; }
		ambient_search_method->setInputCloud(ambient_pointcloud);
	}
	localization_times_msg_.surface_normal_estimation_time = performance_timer.getElapsedTimeInMilliSec();


	// ==============================================================  keypoint selection
	performance_timer.restart();
	typename pcl::PointCloud<PointT>::Ptr ambient_pointcloud_keypoints(new pcl::PointCloud<PointT>());
	if (!ambient_cloud_keypoint_detectors_.empty()) {
		if (!applyKeypointDetection(ambient_cloud_keypoint_detectors_, ambient_pointcloud, ambient_search_method, ambient_pointcloud_keypoints)) { return false; }
	}
	localization_diagnostics_msg_.number_keypoints_ambient_pointcloud = ambient_pointcloud_keypoints->points.size();
	localization_times_msg_.keypoint_selection_time = performance_timer.getElapsedTimeInMilliSec();


	// ==============================================================  feature cloud registration
	performance_timer.restart();
	if (!featurecloud_matchers_.empty() && ros::Time::now() - last_accepted_pose_time_ > pose_tracking_timeout_) { // lost tracking -> try to find initial pose with feature matching
		if (!applyCloudRegistration(featurecloud_matchers_, ambient_pointcloud, ambient_search_method, ambient_pointcloud_keypoints->points.empty() ? ambient_pointcloud : ambient_pointcloud_keypoints, pointcloud_pose_corrected_out)) { return false; }
	}
	localization_times_msg_.featurecloud_registration_time = performance_timer.getElapsedTimeInMilliSec();

	// ==============================================================  point cloud registration
	performance_timer.restart();
	if (!applyCloudRegistration(pointcloud_matchers_, ambient_pointcloud, ambient_search_method, ambient_pointcloud_keypoints->points.empty() ? ambient_pointcloud : ambient_pointcloud_keypoints, pointcloud_pose_corrected_out)) { return false; }
	localization_times_msg_.pointcloud_registration_time = performance_timer.getElapsedTimeInMilliSec();


	// ==============================================================  outlier detection
	performance_timer.restart();
	outlier_percentage_ = applyOutlierDetection(ambient_pointcloud);
	localization_times_msg_.outlier_detection_time = performance_timer.getElapsedTimeInMilliSec();


	// ==============================================================  localization post processors
	performance_timer.restart();
	if (!applyTransformationValidators(pointcloud_pose_initial_guess, pointcloud_pose_corrected_out, outlier_percentage_)) { return false; }
	localization_times_msg_.transformation_validators_time = performance_timer.getElapsedTimeInMilliSec();
	last_accepted_pose_ = pointcloud_pose_corrected_out;
	last_accepted_pose_time_ = ros::Time::now();
	last_accepted_pose_valid_ = true;

	return true;
}
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   </Localization-functions>  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// =============================================================================  </public-section>  ===========================================================================



// =============================================================================   <protected-section>   =======================================================================
// =============================================================================   </protected-section>  =======================================================================



// =============================================================================   <private-section>   =========================================================================
// =============================================================================   </private-section>  =========================================================================

} /* namespace dynamic_robot_localization */
