/***************************************************************************
 *  light-signal-detection.cpp - provides ground truth light signal detection of the nearest achine in front of the robotino
 *  Created: Mon Mar 30 16:28:38 2015
 *  Copyright  2015  Frederik Zwilling
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include <math.h>
#include <fnmatch.h>

#include "tag-vision.h"

using namespace gazebo;

// Register this plugin to make it available in the simulator
GZ_REGISTER_MODEL_PLUGIN(TagVision)

///Constructor
TagVision::TagVision()
{
}
///Destructor
TagVision::~TagVision()
{
  printf("Destructing TagVision Plugin!\n");
}

/** on loading of the plugin
 * @param _parent Parent Model
 */
void TagVision::Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/) 
{
  // Store the pointer to the model
  this->model_ = _parent;

  //get the model-name
  this->name_ = model_->GetName();
  printf("Loading TagVision Plugin of model %s\n", name_.c_str());

  // Listen to the update event. This event is broadcast every
  // simulation iteration.
  this->update_connection_ = event::Events::ConnectWorldUpdateBegin(boost::bind(&TagVision::OnUpdate, this, _1));

  //Create the communication Node for communication with fawkes
  this->node_ = transport::NodePtr(new transport::Node());
  //the namespace is set to the model name!
  this->node_->Init(model_->GetWorld()->GetName()+"/"+name_);

  //Create the communication Node in gazbeo
  this->world_node_ = transport::NodePtr(new transport::Node());
  //the namespace is set to the world name!
  this->world_node_->Init(model_->GetWorld()->GetName());


  //init last sent time
  last_sent_time_ = model_->GetWorld()->GetSimTime().Double();
  last_searched_for_new_tags_time_ = model_->GetWorld()->GetSimTime().Double();

  //create publisher
  result_pub_ = this->node_->Advertise<msgs::PosesStamped>(TAG_VISION_RESULT_TOPIC);
  
  robot_pose_ = model_->GetWorldPose();
}


/** Called by the world update start event
 */
void TagVision::OnUpdate(const common::UpdateInfo & /*_info*/)
{
  double time = model_->GetWorld()->GetSimTime().Double();

  if(time - last_searched_for_new_tags_time_ > SEARCH_FOR_TAGS_INTERVAL)
  {
    last_searched_for_new_tags_time_ = time;
    //check if there are new tags
    unsigned int modelCount = model_->GetWorld()->GetModelCount();
    physics::ModelPtr tmp;
    for(unsigned int i = 0 ; i < modelCount; i++)
    {
      tmp = model_->GetWorld()->GetModel(i);
      if (fnmatch("*tag_*",tmp->GetName().c_str(),FNM_CASEFOLD) == 0)
      {
	//add tag if not already added
	if(tag_poses_.find(tmp) == tag_poses_.end())
	{
	  // printf("TagVision: found new tag: %s\n", tmp->GetName().c_str());
	  tag_poses_[tmp] = tmp->GetWorldPose();
	}
      }
    }
  }

  robot_pose_ = model_->GetWorldPose();
  if(time - last_sent_time_ > SEND_INTERVAL)
  {
    last_sent_time_ = time;
    //compute tag-vision result
    for (std::map<physics::ModelPtr, math::Pose>::iterator it=tag_poses_.begin(); it!=tag_poses_.end(); it++)
    {
      it->second = it->first->GetWorldPose();
      math::Pose rel_pos = it->second - robot_pose_;
      //check if tag is in range, in the camera field of view and faced to the robot
      if(rel_pos.pos.GetLength() < MAX_VIEW_DISTANCE
	 && rel_pos.pos.x > 0 && std::abs(std::asin(rel_pos.pos.Normalize().y)) < CAMERA_FOV / 2.0
	 && std::abs(rel_pos.rot.GetYaw()) > 1.57)
      {
	printf("TagVision: seeing tag %s\n pos: %f,%f angle: %f facing: (%f,%f,%f)\n\n", it->first->GetName().c_str(),
	       rel_pos.pos.x, rel_pos.pos.y, std::asin(rel_pos.pos.Normalize().y), rel_pos.rot.GetRoll(), rel_pos.rot.GetPitch(), rel_pos.rot.GetYaw());
      }
    }
  }
}

/** on Gazebo reset
 */
void TagVision::Reset()
{
}


/** Extract the tag-id from the model name of the tag
 * @param name model-name of the tag
 */ 
int TagVision::get_tag_id_from_name(std::string name)
{
  if(name_.find("tag_") == std::string::npos){
    printf("Tag-Vision: can not get tag-id because the model name of %s has not the format 'prefix/tag_01/suffix'!!\n", name_.c_str());
    return 0;
  }
  std::string tag_id = name_.substr(name_.find("tag_") + 4);
  if(tag_id.find("/") != std::string::npos){
    tag_id = tag_id.substr(0, tag_id.find("/"));
  }
  return std::stoi(tag_id);
}
