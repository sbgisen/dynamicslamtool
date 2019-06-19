#include "CC/CloudCorrespondence.h"
extern visualization_msgs::Marker mark_cluster(pcl::PointCloud<pcl::PointXYZI>::Ptr cloud_cluster, int id, std::string f_id, std::string ns="bounding_box", float r=0.5, float g=0.5, float b=0.5);
extern vector<double> getDisplacementVector(vector<vector<double>> &f1,vector<vector<double>> &f2, map<int,pair<int,double>> &mp);
extern vector<long> getClusterPointcloudChangeVector(vector<pcl::PointCloud<pcl::PointXYZI>::Ptr> &c1,std::vector<pcl::PointCloud<pcl::PointXYZI>::Ptr> &c2, map<int,pair<int,double>> &mp,float resolution = 0.3f);
extern tf::Transform getTransformFromPose(tf::Pose &p1,tf::Pose &p2);
extern vector<double> getFDValuesVector(vector<pcl::PointCloud<pcl::PointXYZI>::Ptr> &c1,std::vector<pcl::PointCloud<pcl::PointXYZI>::Ptr> &c2, map<int,pair<int,double>> &mp);

ros::Publisher pub,marker_pub;
boost::shared_ptr<CloudCorrespondence> ca,cb;
boost::shared_ptr<CloudCorrespondenceMethods> mth;

void moving_object_test(const sensor_msgs::PointCloud2ConstPtr& input, const nav_msgs::OdometryConstPtr& odm)
{
	sensor_msgs::PointCloud2 output;
  	pcl::PCLPointCloud2 *cloud = new pcl::PCLPointCloud2;
  	pcl_conversions::toPCL(*input, *cloud);
  
  	ca = cb;
  	cb.reset(new CloudCorrespondence());
  	pcl::fromPCLPointCloud2(*cloud, *(cb->cloud));
  	tf::poseMsgToTF(odm->pose.pose,cb->ps);
	cb->init = true;
	if(ca->init  == true && cb->init == true)
	{
		tf::Transform t = getTransformFromPose(ca->ps,cb->ps);
		pcl::PointCloud<pcl::PointXYZI> temp = *ca->cloud;
	  	pcl_ros::transformPointCloud(temp,*ca->cloud,t);
		ca->filterPassThrough(5.0,5.0,5.0);
		ca->computeClusters(0.1,"single_cluster");
		cb->filterPassThrough(5.0,5.0,5.0);
		cb->computeClusters(0.1,"single_cluster");
		pcl::toPCLPointCloud2(*(ca->cluster_collection),*cloud);
		pcl_conversions::fromPCL(*cloud, output);
		output.header.frame_id = "previous";
		pub.publish(output);
		pcl::toPCLPointCloud2(*(cb->cluster_collection),*cloud);
		pcl_conversions::fromPCL(*cloud, output);
		output.header.frame_id = "current";
		pub.publish(output);
  		
	  	map<int,pair<int,double>> mp;
	  	mth->calculate_correspondence_centroidKdtree(ca->feature_bank,cb->feature_bank,mp,0.1); //80:kdtree_chi^2,120:dtw
	  	//mth->calculate_correspondence_ESFkdtree(ca->clusters,cb->clusters,mp,0.05);
	  	//vector<double> param_vec = getDisplacementVector(ca->feature_bank,cb->feature_bank,mp);
	  	vector<double> param_vec = getFDValuesVector(ca->clusters,cb->clusters,mp);
	  	//vector<long> param_vec = getClusterPointcloudChangeVector(ca->clusters,cb->clusters,mp,0.3);
	  	float rs=0.4,gs=0.6,bs=0.8,rd=0.8,gd=0.1,bd=0.4;int id = 1;
	  	for(map<int,pair<int,double>>::iterator it=mp.begin();it!=mp.end();it++)
		{
			cout<<"{"<<it->second.first<<"->"<<it->first<<"} Fit Score: "<<it->second.second<<" Moving_Score: "<<param_vec[id-1]<<endl;
			if(param_vec[id-1]>0.1)
			{
				marker_pub.publish(mark_cluster(ca->clusters[it->first],id,"previous","bounding_box",rd,gd,bd));
				marker_pub.publish(mark_cluster(cb->clusters[it->second.first],id,"current","bounding_box",rd,gd,bd));
			}
			else
			{
				marker_pub.publish(mark_cluster(ca->clusters[it->first],id,"previous","bounding_box",rs,gs,bs));
				marker_pub.publish(mark_cluster(cb->clusters[it->second.first],id,"current","bounding_box",rs,gs,bs));
			}
			id++;
		}
		cout<<"-----------------------------------------------------\n";
	}
}

int main (int argc, char** argv)
{
  ros::init (argc, argv, "test_moving_object");
  ros::NodeHandle nh;
  pub = nh.advertise<sensor_msgs::PointCloud2> ("output", 10);
  marker_pub = nh.advertise<visualization_msgs::Marker>("bbox", 10);

  message_filters::Subscriber<sensor_msgs::PointCloud2> pc_sub(nh, "/velodyne_points", 1);
  message_filters::Subscriber<nav_msgs::Odometry> odom_sub(nh, "/camera/odom/sample", 1);
  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::PointCloud2, nav_msgs::Odometry> MySyncPolicy;
  message_filters::Synchronizer<MySyncPolicy> sync(MySyncPolicy(10), pc_sub, odom_sub);
  sync.registerCallback(boost::bind(&moving_object_test, _1, _2));

  ca.reset(new CloudCorrespondence());
  cb.reset(new CloudCorrespondence());
  mth.reset(new CloudCorrespondenceMethods());
  ros::spin();
}