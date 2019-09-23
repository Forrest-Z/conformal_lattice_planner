/*
 * Copyright [2019] [Ke Sun]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <chrono>
#include <unordered_set>
#include <ros/agents_lane_following_node.h>

using namespace router;
using namespace planner;

namespace carla {

bool AgentsLaneFollowingNode::initialize() {

  bool all_param_exist = true;

  std::string host = "localhost";
  int port = 2000;
  all_param_exist &= nh_.param<std::string>("host", host, "localhost");
  all_param_exist &= nh_.param<int>("port", port, 2000);

  // Get the world.
  ROS_INFO_NAMED("agents_lane_following_planner", "connect to the server.");
  client_ = boost::make_shared<CarlaClient>(host, port);
  client_->SetTimeout(std::chrono::seconds(10));
  client_->GetWorld();

  // Initialize the planner.
  ROS_INFO_NAMED("agents_lane_following_planner", "initialize lane following planner.");
  double fixed_delta_seconds = 0.05;
  all_param_exist &= nh_.param<double>("fixed_delta_seconds", fixed_delta_seconds, 0.05);
  planner_ = boost::make_shared<LaneFollower>(fixed_delta_seconds);

  // Start the action server.
  ROS_INFO_NAMED("agents_lane_following_planner", "start action server.");
  server_.start();

  ROS_INFO_NAMED("agents_lane_following_planner", "initialization finishes.");
  return all_param_exist;
}

void AgentsLaneFollowingNode::executeCallback(
    const conformal_lattice_planner::AgentPlanGoalConstPtr& goal) {
  ROS_INFO_NAMED("agents_lane_following_planner", "executeCallback()");

  // Get the ego ID.
  const size_t ego_id = goal->ego_policy.id;

  // Get the agents IDs.
  std::unordered_set<size_t> agent_ids;
  for (const auto& policy : goal->agent_policies)
    agent_ids.insert(policy.id);

  // Update the world for the planner.
  SharedPtr<CarlaWorld> world = boost::make_shared<CarlaWorld>(client_->GetWorld());
  planner_->updateWorld(world);

  // Update the router for the planner.
  // FIXME: Not really need to do this now.
  planner_->updateRouter(LoopRouter());

  // Update the traffic lattice for the planner.
  std::unordered_set<size_t> all_ids = agent_ids;
  all_ids.insert(ego_id);
  planner_->updateTrafficLattice(all_ids);

  //if (planner_->trafficLattice()->vehicles().size() < all_ids.size())
  //  ROS_WARN_NAMED("ego_lane_following_planner", "missing vehicle.");

  // Plan for every agent vehicle.
  for (const auto& policy : goal->agent_policies)
    planner_->plan(policy.id, policy.desired_speed);

  // Inform the client the result of plan.
  conformal_lattice_planner::AgentPlanResult result;
  result.success = true;
  server_.setSucceeded(result);

  return;
}
} // End namespace carla.