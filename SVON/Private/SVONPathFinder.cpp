#include <algorithm>

#include "SVONPathFinder.h"
#include "SVONNode.h"
#include "SVONVolume.h"

using namespace SVON;

bool SVONPathFinder::FindPath(const SVONLink& aStart, const SVONLink& aTarget,
	const FloatVector& aStartPos, const FloatVector& aTargetPos, 
	float agentSize,
	std::vector<SVONPathPoint>& oPoints)
{
	openList.clear();
	closedList.clear();
	cameFrom.clear();
	fScore.clear();
	gScore.clear();
	current = SVONLink();
	goal = aTarget;
	start = aStart;

	openList.push_back(aStart);
	cameFrom.insert(LinksMap::value_type(aStart, aStart));
	gScore.insert(LinkScoreMap::value_type(aStart, 0.0f));
	fScore.insert(LinkScoreMap::value_type(aStart, HeuristicScore(aStart, goal)));// Distance to target

	int numInterations = 0;

	while (openList.size() > 0)
	{
		float lowestScore = FLT_MAX;
		LinkList::const_iterator currentIt = openList.cend();
		LinkList::const_iterator cit = openList.cbegin();
		for (; cit != openList.cend(); ++cit)
		{
			const SVONLink& link = *cit;

			//>> Logic modifed by ray
			//if (fScore.find(link) == fScore.end()
			//	|| fScore[link] < lowestScore)
			if (fScore.find(link) != fScore.end()
				&& fScore[link] < lowestScore)
			//<< Logic modifed by ray
			{
				lowestScore = fScore[link];
				current = link;

				currentIt = cit;
			}
		}

		openList.erase(currentIt);
		closedList.push_back(current);

		if (current == goal)
		{
			BuildPath(cameFrom, current, aStartPos, aTargetPos, oPoints);
			return true;
		}

		const SVONNode& currentNode = volume.GetNode(current);

		std::vector<SVONLink> neighbours;
		if (current.GetLayerIndex() == 0 && currentNode.firstChild.IsValid())
		{
			volume.GetLeafNeighbours(current, agentSize, neighbours);
		}
		else
		{
			volume.GetNeighbours(current, agentSize, neighbours);
		}

		for (const SVONLink& neighbour : neighbours)
		{
			ProcessLink(neighbour);
		}

		++numInterations;
	}

	return false;
}

float SVONPathFinder::HeuristicScore(const SVONLink& aStart, const SVONLink& aTarget)
{
	float score = 0.0f;

	FloatVector startPos, endPos;
	volume.GetLinkPosition(aStart, startPos);
	volume.GetLinkPosition(aTarget, endPos);
	switch (settings.pathCostType)
	{
	case SVONPathCostType::MANHATTAN:
		score = abs(endPos.X - startPos.X)
			+ abs(endPos.Y - startPos.Y)
			+ abs(endPos.Z - startPos.Z);
		break;
	case SVONPathCostType::EUCLIDEAN:
	default:
		score = (startPos - endPos).Size();
		break;
	}

	float layerIndex = static_cast<float>(aTarget.GetLayerIndex());
	float layerCount = static_cast<float>(volume.GetNumLayers());
	float compensation = (1.0f - layerIndex / layerCount) * settings.nodeSizeCompensation;
	score *= compensation;

	return score;
}

float SVONPathFinder::GetCost(const SVONLink& aStart, const SVONLink& aTarget)
{
	float cost = 0.0f;

	// Unit cost implementation
	if (settings.useUnitCost)
	{
		cost = settings.unitCost;
	}
	else
	{
		FloatVector startPos(0.0f), endPos(0.0f);
		volume.GetLinkPosition(aStart, startPos);
		volume.GetLinkPosition(aTarget, endPos);
		cost = (startPos - endPos).Size();
	}

	float layerIndex = static_cast<float>(aTarget.GetLayerIndex());
	float layerCount = static_cast<float>(volume.GetNumLayers());
	float compensation = (1.0f - layerIndex / layerCount) * settings.nodeSizeCompensation;
	cost *= compensation;

	return cost;
}

void
SVONPathFinder::ProcessLink(const SVONLink& aNeighbour)
{
	if (aNeighbour.IsValid())
	{
		if (std::find(closedList.begin(), closedList.end(), aNeighbour) != closedList.end())
		{
			return;
		}

		if (std::find(openList.begin(), openList.end(), aNeighbour) == openList.end())
		{
			// Add to openList
			openList.push_back(aNeighbour);
		}

		float t_gScore = FLT_MAX;
		auto currentGScoreIt = gScore.find(current);
		if (currentGScoreIt != gScore.end())
		{
			t_gScore = currentGScoreIt->second + GetCost(current, aNeighbour);
		}
		else
		{
			gScore.insert(LinkScoreMap::value_type(current, FLT_MAX));
		}

		auto neighbourGScoreIt = gScore.find(aNeighbour);
		auto neighbourGScore = neighbourGScoreIt != gScore.end() ? neighbourGScoreIt->second : FLT_MAX;
		if (t_gScore >= neighbourGScore)
		{
			return;
		}

		cameFrom.insert(LinksMap::value_type(aNeighbour, current));
		gScore.insert(LinkScoreMap::value_type(aNeighbour, t_gScore));

		auto newFScore = t_gScore + settings.estimateWeight * HeuristicScore(aNeighbour, goal);
		fScore.insert(LinkScoreMap::value_type(aNeighbour, newFScore));
	}
}

void SVONPathFinder::BuildPath(LinksMap& aCameFrom, SVONLink aCurrent,
	const FloatVector& aStartPos, const FloatVector& aTargetPos,
	std::vector<SVONPathPoint>& oPoints)
{
	//>> Logic modified by Ray: add position of the voxel center that is closest to target position
	AddPathPoint(oPoints, aCurrent);// the positon of first point will be changed to TargetPos later
	//<< Logic modified by Ray

	LinksMap::const_iterator cit = aCameFrom.find(aCurrent);
	while (cit != aCameFrom.cend()
		&& cit->second != aCurrent)
	{
		aCurrent = cit->second;
		AddPathPoint(oPoints, aCurrent);

		cit = aCameFrom.find(aCurrent);
	}

	auto nPoints = oPoints.size();
	if (nPoints > 1)
	{
		oPoints[0].position = aTargetPos;
		oPoints[nPoints - 1].position = aStartPos;
	}
	else // If start and end are in the same voxel, just use the start and target positions
	{
		// nPoints == 1
		//>> Logic modified by Ray: aleady added at least one position, do not need this anymore
		//if (nPoints == 0)
		//{
		//	oPoints.push_back(SVONPathPoint());
		//}
		//<< Logic modified by Ray

		oPoints[0].position = aTargetPos;
		oPoints.push_back(SVONPathPoint(aStartPos, start.GetLayerIndex(), 0));
	}

	reverse(oPoints.begin(), oPoints.end());
}

void SVONPathFinder::AddPathPoint(std::vector<SVONPathPoint>& points, SVONLink aPointLink)
{
	SVONPathPoint pos;

	volume.GetLinkPosition(aPointLink, pos.position);

	const SVONNode& node = volume.GetNode(aPointLink);
	pos.code = node.code;

	//This is rank, I really should sort the layers out
	// Comment by Ray: Here, layer 0 means subnode, the author really should unify the standard
	// Which means: when generating the volume, the layer 0 means leafnode layer, no layer for subnode
	// But here(get the path point), the layer 0 means subnode...

	if (aPointLink.GetLayerIndex() == 0)
	{
		if (!node.HasChildren())
		{
			pos.layer = 1;
		}
		else
		{
			pos.layer = 0;
		}
	}
	else
	{
		pos.layer = aPointLink.GetLayerIndex() + 1;
	}

	points.push_back(pos);
}