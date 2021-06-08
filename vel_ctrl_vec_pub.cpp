#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/debug_vect.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <iostream>
#include <chrono>
#include <math.h>  

using namespace std::chrono_literals;

//creates a VelocityControlVectorAdvertiser class that subclasses the generic rclcpp::Node base class.
class VelocityControlVectorAdvertiser : public rclcpp::Node
{

//Creates a function for when messages are to be sent. 
//Messages are sent based on a timed callback.
	public:
		VelocityControlVectorAdvertiser() : Node("vel_ctrl_vect_advertiser") {
			publisher_ = this->create_publisher<px4_msgs::msg::DebugVect>("vel_ctrl_vect_topic", 10);
			
			subscription_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
			"/dist_sensor/laser_scan",	10,
			std::bind(&VelocityControlVectorAdvertiser::OnSensorMsg, this, std::placeholders::_1));
		}

	private:
		rclcpp::TimerBase::SharedPtr timer_;
		rclcpp::Publisher<px4_msgs::msg::DebugVect>::SharedPtr publisher_;
		rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
		int callback_count = 0;
		void OnSensorMsg(const sensor_msgs::msg::LaserScan::SharedPtr _msg);
		void VelocityDroneControl(float xv, float yv, float zv);
};


// Control drone velocities
void VelocityControlVectorAdvertiser::VelocityDroneControl(float xv, float yv, float zv){
	auto vel_ctrl_vect = px4_msgs::msg::DebugVect();
	vel_ctrl_vect.timestamp = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
	std::string name = "test";
	std::copy(name.begin(), name.end(), vel_ctrl_vect.name.begin());
	vel_ctrl_vect.x = xv;
	vel_ctrl_vect.y = yv;
	vel_ctrl_vect.z = zv;
	this->publisher_->publish(vel_ctrl_vect);
}


// Lidar message callback function
void VelocityControlVectorAdvertiser::OnSensorMsg(const sensor_msgs::msg::LaserScan::SharedPtr _msg){
	float angle_increment = _msg->angle_increment;
	float angle_min = _msg->angle_min;
	float angle_max = _msg->angle_max;
	float total_angle = angle_max - angle_min;
	int num_of_rays = round(total_angle / angle_increment); // 1000
	
	// get shortest dist index
	int shortestDistIdx = 0;
	for(int i = 0; i < num_of_rays; i++){
		if(_msg->ranges[shortestDistIdx] > _msg->ranges[i]){
			shortestDistIdx = i;
		}
	}
	
	// angle compared to straight up from drone
	float shortestDistIdxAngle = float(shortestDistIdx)*angle_increment - angle_max; 
	std::cout << "angle: " << shortestDistIdxAngle << std::endl;
	
	auto vel_ctrl_vect = px4_msgs::msg::DebugVect();
	vel_ctrl_vect.timestamp = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()).time_since_epoch().count();
	std::string name = "test";
	std::copy(name.begin(), name.end(), vel_ctrl_vect.name.begin());
	
	if(_msg->ranges[shortestDistIdx] < 10){
		// create velocity control vector to steer drone towards cable
		if(_msg->ranges[shortestDistIdx] > 1){
			VelocityDroneControl(0, shortestDistIdxAngle, -0.1*_msg->ranges[shortestDistIdx]);
		} else if(_msg->ranges[shortestDistIdx] < 1){
			VelocityDroneControl(0, shortestDistIdxAngle, 0.1*_msg->ranges[shortestDistIdx]);
		} else {
			VelocityDroneControl(0,shortestDistIdxAngle,0);
		}
	} else {	
		// control drone in square motion if nothing within lidar fov
		static int callbackscale = 5;
		std::cout << "CallbackCount" << callback_count << std::endl;
		if(callback_count < 100*callbackscale){
			VelocityDroneControl(NAN,NAN,-3.0); // go up
			std::cout << "START" << std::endl;
		} else if(callback_count % (100*callbackscale) < 25*callbackscale){
			VelocityDroneControl(0,3,0); // go east
			std::cout << "EAST" << std::endl;
		} else if(callback_count % (100*callbackscale) < 50*callbackscale){
			VelocityDroneControl(0,0,-3); // go up
			std::cout << "UP" << std::endl;
		} else if(callback_count % (100*callbackscale) < 75*callbackscale){
			VelocityDroneControl(0,-3,0); // go west
			std::cout << "WEST" << std::endl;
		} else if(callback_count % (100*callbackscale) > 74*callbackscale){
			VelocityDroneControl(0,0,3); // go down
			std::cout << "DOWN" << std::endl;
		} else{
			VelocityDroneControl(0,0,0); // do nothing
		}	
		callback_count++;
	}

	
}

			
int main(int argc, char *argv[])
{
	std::cout << "Starting vel_ctrl_vect advertiser node..." << std::endl;
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<VelocityControlVectorAdvertiser>());

	rclcpp::shutdown();
	return 0;
}
