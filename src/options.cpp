/*
 * options.cpp
 *
 *  Created on: 04 апр. 2014 г.
 *      Author: kisame
 */

#include "options.h"
#include "config.h"

Options::Options() {
	// TODO Auto-generated constructor stub
	m_cSolverInfo = nullptr;
}

Options::~Options() {
	// TODO Auto-generated destructor stub
}

void Options::setSolverInfo(SolverInfo* _SolverInfo) {
	m_cSolverInfo = _SolverInfo;
}

SolverInfo* Options::getSolverInfo() {
	return m_cSolverInfo;
}

void Options::Init() {
  // Feel free to add your own configuration
  // All the missed parameters will be filled with default
  
  // Debug configuration 1
  std::shared_ptr<Config> pDebugConfig1(new Config("debug_1"));
  pDebugConfig1->SetGridSize(Vector3i(10, 10, 1));
  pDebugConfig1->SetGridGeometryType(sep::DEBUG1_GRID_GEOMETRY);
  pDebugConfig1->SetMaxIteration(5);
  pDebugConfig1->SetUseIntegral(true);
  AddConfig(pDebugConfig1);
  
  // Bigger grid
  std::shared_ptr<Config> pDebugConfig2(new Config("more_bigger"));
  pDebugConfig2->SetGridSize(Vector3i(8, 8, 1));
  pDebugConfig2->SetGridGeometryType(sep::DIMAN_GRID_GEOMETRY);
  pDebugConfig2->SetMaxIteration(100);
  pDebugConfig2->SetUseIntegral(true);
  AddConfig(pDebugConfig2);
  
  // Finally, select wich is active
//  SetActiveConfig("debug_1");
  SetActiveConfig("more_bigger");
}

void Options::AddConfig(std::shared_ptr<Config> pConfig) {
  m_mConfigs[pConfig->GetName()] = pConfig;
}

bool Options::SetActiveConfig(const std::string& sName) {
  if (m_mConfigs.find(sName) == m_mConfigs.end())
    return false;
  m_sActiveConfig = sName;
  try {
    std::cout << "Activating config: ";
    GetConfig()->PrintMe();
  } catch (char const* sExc) {
    std::cout << "Exception occurs: " << sExc << std::endl;
    return false;
  }
  return true;
}

Config* Options::GetConfig() {
  if (m_mConfigs.find(m_sActiveConfig) == m_mConfigs.end())
    throw "No active config yet";
  return m_mConfigs[m_sActiveConfig].get();
}
