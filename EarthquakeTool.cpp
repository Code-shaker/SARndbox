/***********************************************************************
EarthquakeTool - Tool to create a circular or planar perturbation in the water 
table to mimic an earthquake.

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "EarthquakeTool.h"

#include <cmath>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <IO/ValueSource.h>
#include <Geometry/GeometryValueCoders.h>
#include <Vrui/OpenFile.h>
#include <Vrui/DisplayState.h>

#include "WaterTable2.h"
#include "EarthquakeManager.h"
#include "Sandbox.h"
#include "Config.h"

#include <stdio.h>

/**************************************
Methods of class EarthquakeToolFactory:
**************************************/

EarthquakeToolFactory::EarthquakeToolFactory(WaterTable2* sWaterTable, EarthquakeManager* sEarthquakeManager, Vrui::ToolManager& toolManager)
	:ToolFactory("EarthquakeTool",toolManager),
	 waterTable(sWaterTable), 
	 earthquakeManager(sEarthquakeManager)
	{
	/* Retrieve bathymetry grid and cell sizes: */
	for(int i=0;i<2;++i)
		gridSize[i]=waterTable->getBathymetrySize(i);
	
	/* Initialize tool layout: */
	layout.setNumButtons(3);
	
	/* Set tool class' factory pointer: */
	EarthquakeTool::factory=this;
	}

EarthquakeToolFactory::~EarthquakeToolFactory(void)
	{
	/* Reset tool class' factory pointer: */
	EarthquakeTool::factory=0;
	}

const char* EarthquakeToolFactory::getName(void) const
	{
	return "Create Earthquake";
	}

const char* EarthquakeToolFactory::getButtonFunction(int buttonSlotIndex) const
	{
	if (buttonSlotIndex == 0)
		return "Create Circular Perturbation";
	else if (buttonSlotIndex == 1)
		return "Set Planar Perturbation Points";
	else return "Create Planar Perturbation";
	}

Vrui::Tool* EarthquakeToolFactory::createTool(const Vrui::ToolInputAssignment& inputAssignment) const
	{
	return new EarthquakeTool(this,inputAssignment);
	}

void EarthquakeToolFactory::destroyTool(Vrui::Tool* tool) const
	{
	delete tool;
	}

/*****************************************************
Static elements and functions of class EarthquakeTool:
*****************************************************/

EarthquakeToolFactory* EarthquakeTool::factory=0;

/* Calculate distance between two points */
static double getDistance(int p1[], int p2[]) {
	return sqrt(pow(p1[0]-p2[0], 2) + pow(p1[1]-p2[1], 2));
}

/* Returns whether the plane point has been initialized */
static bool hasPlanePoint(int p[]) {
	return !(p[0]==0 && p[1]==0 && p[2]==0);
}

/* Determines if the given point is within the given bounds */
static bool inBounds(int p[], int bounds[]) {
	return (p[0] >= 0 && p[0] < bounds[0] &&
	       p[1] >= 0 && p[1] < bounds[1]);
}

/*******************************
Methods of class EarthquakeTool:
*******************************/

void EarthquakeTool::createCircularPerturbation(int center[], int radius, double perturbation)
	{
	GLfloat *pPtr = bathymetryBuffer;
	/* For all pixel columns */
	for (int i = 0; i < factory->gridSize[0]-1; i++) {
		pPtr = bathymetryBuffer + i;
		
		/* Iterate through all values in that column */
		for (int j = 0; j < factory->gridSize[1]-1; j++) {
			pPtr += factory->gridSize[0];
			
			/* Perturb the pixel if it's inside the circle */
			int p2[2] = {i, j};
			if (getDistance(center, p2) <= radius)
				*pPtr += perturbation;
		}
	}
	}
	
void EarthquakeTool::createPlanarPerturbation(int p1[], int p2[], double perturbation)
	{
	float slope = (p1[1]-p2[1])/((float) p1[0]-p2[0]);
	float yint = p2[1]-slope*p2[0];
	
	GLfloat *pPtr = bathymetryBuffer;
	/* For all pixel columns */
	for (int i = 0; i < factory->gridSize[0]-1; i++) {
		pPtr = bathymetryBuffer + i;
		
		/* Iterate through all values in that column */
		for (int j = 0; j < factory->gridSize[1]-1; j++) {
			pPtr += factory->gridSize[0];
			
			/* Perturb the pixel if it's inside the plane. Determine the 
		         direction of propogation based on the order the points were chosen. */
			float y = i*slope + yint;
			if ((p1[1] < p2[1] && j <= y) || (p1[1] > p2[1] && j >= y)) 
				*pPtr += perturbation;
			else if (p1[1]==p2[1] && ((p1[0] < p2[0] && j <= y) || (p1[0] > p2[0] && j >= y))) 
				*pPtr += perturbation;
		}
	}
	}
	
int EarthquakeTool::getPixelPos(OPoint p1, const char comp)
{
	if (comp == 'x')
		{
		/* Average the pixel position calculations for the top and bottom 
		corners since they might not be in line */
		double x_top = (p1[0] - basePlaneCorners[1][0])/(basePlaneCorners[0][0] - basePlaneCorners[1][0]);
		double x_bot = (p1[0] - basePlaneCorners[3][0])/(basePlaneCorners[2][0] - basePlaneCorners[3][0]);
		double pix_top = x_top * factory->gridSize[0];
		double pix_bot = x_bot * factory->gridSize[0];
		int x_pix = round((pix_top + pix_bot)/2);
		return x_pix;
		}
	if (comp == 'y')
		{
		/* Average the pixel position calculations for the left and right
		corners since they might not be in line */
		double y_left = (p1[1] - basePlaneCorners[3][1])/(basePlaneCorners[1][1] - basePlaneCorners[3][1]);
		double y_right = (p1[1] - basePlaneCorners[2][1])/(basePlaneCorners[0][1] - basePlaneCorners[2][1]);
		double pix_left = y_left * factory->gridSize[1];
		double pix_right = y_right * factory->gridSize[1];
		int y_pix = round((pix_left + pix_right)/2);
		return y_pix;
		}
	
	return 0;
}

EarthquakeToolFactory* EarthquakeTool::initClass(WaterTable2* sWaterTable, EarthquakeManager *sEarthquakeManager, Vrui::ToolManager& toolManager)
	{	
	/* Create the tool factory: */
	factory=new EarthquakeToolFactory(sWaterTable,sEarthquakeManager, toolManager);
	
	/* Register and return the class: */
	toolManager.addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	return factory;
	}

EarthquakeTool::EarthquakeTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment),
	 bathymetryBuffer(new GLfloat[EarthquakeTool::factory->gridSize[1]*EarthquakeTool::factory->gridSize[0]]),
	 requestPending(false), isCircular(false), isPlanar(false)
	{
	
	/* Get the sandbox layout filename */
	std::string sandboxLayoutFileName=CONFIG_CONFIGDIR;
	sandboxLayoutFileName.push_back('/');
	sandboxLayoutFileName.append(CONFIG_DEFAULTBOXLAYOUTFILENAME);

	/* Read the sandbox layout file: */
	{
	IO::ValueSource layoutSource(Vrui::openFile(sandboxLayoutFileName.c_str()));
	layoutSource.skipWs();
	std::string s=layoutSource.readLine();
	for(int i = 0; i < 4; ++i)
		{
		layoutSource.skipWs();
		s=layoutSource.readLine();
		basePlaneCorners[i]=Misc::ValueCoder<OPoint>::decode(s.c_str(),s.c_str()+s.length());
		}
	}
	
	/* Initialize plane points */
	for(int i = 0; i < 2; i++) 
		for (int j = 0; j < 3; j++)
			planePoints[i][j] = 0;
		
	}

EarthquakeTool::~EarthquakeTool(void)
	{
	delete[] bathymetryBuffer;
	}

const Vrui::ToolFactory* EarthquakeTool::getFactory(void) const
	{
	return factory;
	}

void EarthquakeTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		/* Request a bathymetry grid from the water table: */
		requestPending=factory->waterTable->requestBathymetry(bathymetryBuffer);
			
		if(buttonSlotIndex==0)
			{
			/* Create circular perturbation */
			isCircular = true;
			isPlanar = false;
			}
		else if (buttonSlotIndex==1)
			{
			isCircular = false;
			isPlanar = false;
			
			/* Get position of cursor in object space and convert to pixels */
			OPoint cursorPos=Vrui::getInverseNavigationTransformation().transform(getButtonDevicePosition(0));
			int p1[3] = {getPixelPos(cursorPos, 'x'), getPixelPos(cursorPos, 'y'), (int) cursorPos[2]};
			
			if (!hasPlanePoint(planePoints[0]))
				{
				/* Initialize first plane point if not already initialized */
				for (int i = 0; i < 3; i++)
					planePoints[0][i] = p1[i];
				}
			else if (!hasPlanePoint(planePoints[1]))
				{
				/* Initialize second plane point if not already initialized */
				for (int i = 0; i < 3; i++)
					planePoints[1][i] = p1[i];
				}
			else 
				{
				/* Update first plane point and reset second plane point */
				for (int i = 0; i < 3; i++) 
					{
					planePoints[0][i] = p1[i];
					planePoints[1][i] = 0;
					}
				}
			}
		else
			{
			/* Create planar perturbation */
			isCircular = false;
			isPlanar = true;
			}
		}
	}

void EarthquakeTool::frame(void)
	{
	if(requestPending&&factory->waterTable->haveBathymetry())
		{
		if (isCircular)
			{
			/* Get position of cursor in object space and convert to pixels */
			OPoint cursorPos=Vrui::getInverseNavigationTransformation().transform(getButtonDevicePosition(0));
			int center[2] = {getPixelPos(cursorPos, 'x'), getPixelPos(cursorPos, 'y')};
		
			/* Make sure pixel center is in bounds of the box */
			if (inBounds(center, (factory->gridSize)))
				{
				/* Get radius and perturbation of earthquake */
				int radius = factory->earthquakeManager->getEarthquakeRadius();
				double perturbation = factory->earthquakeManager->getEarthquakePerturbation();
				
				/* Update the bathymetry grid: */
				createCircularPerturbation(center, radius, perturbation);

				/* Set bathymetry grid to updated bathymetry buffer */
				factory->earthquakeManager->setBathymetryGrid(bathymetryBuffer);
				}
			}
		else if (isPlanar)
			{
			/* Make sure points are initialized and in bounds of the box */
			if (hasPlanePoint(planePoints[0]) && hasPlanePoint(planePoints[1]) &&
			    inBounds(planePoints[0], factory->gridSize) && inBounds(planePoints[1], factory->gridSize))
				{
				/* Get perturbation of earthquake */
				double perturbation = factory->earthquakeManager->getEarthquakePerturbation();
		
				/* Update the bathymetry grid: */
				createPlanarPerturbation(planePoints[0], planePoints[1], perturbation);

				/* Set bathymetry grid to updated bathymetry buffer */
				factory->earthquakeManager->setBathymetryGrid(bathymetryBuffer);
				}
			}
		requestPending=false;
		}
	}
