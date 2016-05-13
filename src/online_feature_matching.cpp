#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include "tfg/dmatch.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <stdio.h>

using namespace cv;

Mat left_img, right_img;

void left_img_callback(const sensor_msgs::ImageConstPtr& msg)
{
	left_img = cv_bridge::toCvCopy(msg, "bgr8")->image;
}

void right_img_callback(const sensor_msgs::ImageConstPtr& msg)
{
	right_img = cv_bridge::toCvCopy(msg, "bgr8")->image;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "online_feature_matching");
    ros::NodeHandle n;
    image_transport::ImageTransport it(n);
    image_transport::Subscriber left_sub = it.subscribe("left/image_raw", 1, left_img_callback);
    image_transport::Subscriber right_sub = it.subscribe("right/image_raw", 1, right_img_callback);

    ros::Publisher matches_pub = n.advertise<tfg::dmatch>("dmatches", 5);

    ros::Rate loop_rate(2);

    while (ros::ok())
	{
		std::vector<cv::KeyPoint> left_keypoints;
    	std::vector<cv::KeyPoint> right_keypoints;
    	Mat left_mask = cv::Mat::ones(left_img.size(), CV_8U);
    	Mat right_mask = cv::Mat::ones(right_img.size(), CV_8U);
    	Mat left_descriptors;
    	Mat right_descriptors;

    	SURF detector(400);
    	detector(left_img, left_mask, left_keypoints, left_descriptors, false);
    	detector(right_img, right_mask, right_keypoints, right_descriptors, false);

    	if(left_descriptors.type() != CV_32F){
        	left_descriptors.convertTo(left_descriptors, CV_32F);
    	}

    	if(right_descriptors.type() != CV_32F){
        	right_descriptors.convertTo(right_descriptors, CV_32F);
    	}

		cv::FlannBasedMatcher matcher;
	    std::vector<std::vector<cv::DMatch> > matches;
	    matcher.knnMatch( left_descriptors, right_descriptors, matches, 2 );

	    double thresholdDist2 = 0.0625 * double(left_img.size().height*left_img.size().height + 
							left_img.size().width*left_img.size().width);

	    vector< cv::DMatch > good_matches2;
		good_matches2.reserve(matches.size());  
		for (size_t i = 0; i < matches.size(); ++i)
		{ 
		    for (int j = 0; j < matches[i].size(); j++)
		    {
	        	Point2f from = left_keypoints[matches[i][j].queryIdx].pt;
	        	Point2f to = right_keypoints[matches[i][j].trainIdx].pt;

				double dist2 = (from.x - to.x) * (from.x - to.x) + (from.y - to.y) * (from.y - to.y);
		
	        	if (dist2 < thresholdDist2 && abs(from.y-to.y)<3)
	        	{
		            good_matches2.push_back(matches[i][j]);
	            	j = matches[i].size();
	        	}
	    	}
		}
		
		tfg::dmatch dmatch_msg;
		dmatch_msg.queryIdx = good_matches2[0].queryIdx;
		dmatch_msg.trainIdx = good_matches2[0].trainIdx;
		dmatch_msg.imgIdx = good_matches2[0].imgIdx;
		dmatch_msg.distance = good_matches2[0].distance;

    	matches_pub.publish(dmatch_msg);

    	ros::spinOnce();
    	loop_rate.sleep();
    }

    return 0;
}