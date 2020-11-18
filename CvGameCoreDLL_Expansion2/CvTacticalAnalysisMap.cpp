/*	-------------------------------------------------------------------------------------------------------
	� 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvGameCoreUtils.h"
#include "CvTacticalAnalysisMap.h"
#include "CvMilitaryAI.h"
#include "cvStopWatch.h"
#include "CvDiplomacyAI.h"
#include <sstream>
#include <iomanip>

#include "LintFree.h"

//=====================================
// CvTacticalDominanceZone
//=====================================
/// Constructor
CvTacticalDominanceZone::CvTacticalDominanceZone(void)
{
	m_iZoneID = -1;
	m_eTerritoryType = TACTICAL_TERRITORY_NONE;
	m_eOverallDominanceFlag = TACTICAL_DOMINANCE_NO_UNITS_VISIBLE;
	m_ePosture = TACTICAL_POSTURE_NONE;
	m_eOwner = NO_PLAYER;
	m_iCityID = -1;
	m_iAreaID = 0;
	m_iFriendlyMeleeStrength = 0;
	m_iEnemyMeleeStrength = 0;
	m_iFriendlyRangedStrength = 0;
	m_iEnemyRangedStrength = 0;
	m_iFriendlyNavalStrength = 0;
	m_iEnemyNavalStrength = 0;
	m_iFriendlyNavalRangedStrength = 0;
	m_iEnemyNavalRangedStrength = 0;
	m_iFriendlyUnitCount = 0;
	m_iEnemyUnitCount = 0;
	m_iNeutralUnitStrength = 0;
	m_iNeutralUnitCount = 0;
	m_iEnemyNavalUnitCount = 0;
	m_iFriendlyNavalUnitCount = 0;
	m_iZoneValue = 0;
	m_iDistanceOfClosestEnemyUnit = -1;

#if defined(MOD_BALANCE_CORE_MILITARY)
	m_iAvgX = m_iAvgY = 0;
	m_iPlotCount = 0;
#endif
}

/// Retrieve city controlling this zone
CvCity* CvTacticalDominanceZone::GetZoneCity() const
{
	if(m_eOwner != NO_PLAYER)
	{
		return GET_PLAYER(m_eOwner).getCity(m_iCityID);
	}

	return NULL;
}

/// Set city controlling this zone
void CvTacticalDominanceZone::SetZoneCity(CvCity* pCity)
{
	if(pCity != NULL)
	{
		m_iCityID = pCity->GetID();
	}
	else
	{
		m_iCityID = -1;
	}
}

eTacticalDominanceFlags CvTacticalDominanceZone::GetRangedDominanceFlag(int iDominancePercentage) const
{
	if ( GetEnemyRangedStrength() <= 0 && GetFriendlyRangedStrength() > 0)
	{
		return TACTICAL_DOMINANCE_FRIENDLY;
	}
	else
	{
		//avoid overflow
		int iRatio = int(0.5f + 100 * ( float(GetFriendlyRangedStrength()) / max(1u, GetEnemyRangedStrength())));
		if (iRatio > 100 + iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_FRIENDLY;
		}
		else if (iRatio < 100 - iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_ENEMY;
		}
	}

	return TACTICAL_DOMINANCE_EVEN;
}

eTacticalDominanceFlags CvTacticalDominanceZone::GetUnitCountDominanceFlag(int iDominancePercentage) const
{
	if (GetTotalEnemyUnitCount() <= 0 && GetTotalFriendlyUnitCount() > 0)
	{
		return TACTICAL_DOMINANCE_FRIENDLY;
	}
	else
	{
		int iRatio = (GetTotalFriendlyUnitCount() * 100) / max(1, GetTotalEnemyUnitCount());
		if (iRatio > 100 + iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_FRIENDLY;
		}
		else if (iRatio < 100 - iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_ENEMY;
		}
	}

	return TACTICAL_DOMINANCE_EVEN;
}

eTacticalDominanceFlags CvTacticalDominanceZone::GetNavalRangedDominanceFlag(int iDominancePercentage) const
{
	if (GetEnemyNavalRangedStrength() <= 0 && GetFriendlyNavalRangedStrength() > 0)
	{
		return TACTICAL_DOMINANCE_FRIENDLY;
	}
	else
	{
		//avoid overflow
		int iRatio = int(0.5f + 100 * ( float(GetFriendlyNavalRangedStrength()) / max(1u, GetEnemyNavalRangedStrength())));
		if (iRatio > 100 + iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_FRIENDLY;
		}
		else if (iRatio < 100 - iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_ENEMY;
		}
	}

	return TACTICAL_DOMINANCE_EVEN;
}

eTacticalDominanceFlags CvTacticalDominanceZone::GetNavalUnitCountDominanceFlag(int iDominancePercentage) const
{
	if (GetEnemyNavalUnitCount() <= 0 && GetFriendlyNavalUnitCount() > 0)
	{
		return TACTICAL_DOMINANCE_FRIENDLY;
	}
	else
	{
		int iRatio = (GetFriendlyNavalUnitCount() * 100) / max(1, GetEnemyNavalUnitCount());
		if (iRatio > 100 + iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_FRIENDLY;
		}
		else if (iRatio < 100 - iDominancePercentage)
		{
			return TACTICAL_DOMINANCE_ENEMY;
		}
	}

	return TACTICAL_DOMINANCE_EVEN;
}


#if defined(MOD_BALANCE_CORE_MILITARY)
void CvTacticalDominanceZone::Extend(CvPlot* pPlot)
{
	if (!pPlot)
		return;

	if (m_iPlotCount==0)
	{
		m_iPlotCount = 1;
		m_iAvgX = pPlot->getX() * 1000;
		m_iAvgY = pPlot->getY() * 1000;
	}
	else
	{
		//need to take care of wrap-around ... should probably do all this in hex-space
		int iHalfWidth = GC.getMap().getGridWidth()*1000/2;
		int iHalfHeight = GC.getMap().getGridHeight()*1000/2;

		int iDX = pPlot->getX()*1000 - m_iAvgX;
		int iDY = pPlot->getY()*1000 - m_iAvgY;

		if (GC.getMap().isWrapX() && iDX < -iHalfWidth)
			iDX += 2*iHalfWidth;
		if (GC.getMap().isWrapX() && iDX > +iHalfWidth)
			iDX -= 2*iHalfWidth;
		if (GC.getMap().isWrapY() && iDY < -iHalfHeight)
			iDY += 2*iHalfHeight;
		if (GC.getMap().isWrapY() && iDY > +iHalfHeight)
			iDY -= 2*iHalfHeight;

		m_iPlotCount++;
		m_iAvgX += iDX/m_iPlotCount;
		m_iAvgY += iDY/m_iPlotCount;

		if (m_iAvgX<0)
			m_iAvgX += iHalfWidth*2;
		if (m_iAvgX>iHalfWidth*2)
			m_iAvgX -= iHalfWidth*2;
		if (m_iAvgY<0)
			m_iAvgY += iHalfHeight*2;
		if (m_iAvgY>iHalfHeight*2)
			m_iAvgY += iHalfHeight*2;
	}
}
#endif

/// Retrieve distance in hexes of closest enemy to center of this zone
int CvTacticalDominanceZone::GetDistanceOfClosestEnemyUnit() const
{
	return m_iDistanceOfClosestEnemyUnit;
}

/// Set distance in hexes of closest enemy to center of this zone
void CvTacticalDominanceZone::SetDistanceOfClosestEnemyUnit(int iRange)
{
	m_iDistanceOfClosestEnemyUnit = iRange;
}

//=====================================
// CvTacticalAnalysisMap
//=====================================
/// Constructor
CvTacticalAnalysisMap::CvTacticalAnalysisMap(void) :
	m_iDominancePercentage(25),
	m_iUnitStrengthMultiplier(1),
	m_iTacticalRangeTurns(6),
	m_ePlayer(NO_PLAYER),
	m_iTurnSliceBuilt(-1)
{
}

/// Destructor
CvTacticalAnalysisMap::~CvTacticalAnalysisMap(void)
{
}

/// Initialize
void CvTacticalAnalysisMap::Init(PlayerTypes ePlayer)
{
	// Time building of these maps
	AI_PERF("AI-perf-tact.csv", "CvTacticalAnalysisMap::Init()" );

	m_ePlayer = ePlayer;
	m_vPlotZoneID = vector<int>( GC.getMap().numPlots(), -1 );
	m_DominanceZones.clear();
	m_iDominancePercentage = GC.getAI_TACTICAL_MAP_DOMINANCE_PERCENTAGE();

	m_iTurnSliceBuilt = -1;
	m_iTacticalRangeTurns = 6;
	m_iUnitStrengthMultiplier = 1;

}

int CvTacticalDominanceZone::GetBorderScore(DomainTypes eDomain, CvCity** ppWorstNeighborCity) const
{
	int iSum = 0;

	if (ppWorstNeighborCity)
		*ppWorstNeighborCity = NULL;
	int iWorstScore = 0;

	CvTacticalAnalysisMap* pTactMap = GET_PLAYER(m_eOwner).GetTacticalAI()->GetTacticalAnalysisMap();
	for (size_t i = 0; i < m_vNeighboringZones.size(); i++)
	{
		CvTacticalDominanceZone* pNeighbor = pTactMap->GetZoneByID(m_vNeighboringZones[i]);
		if (!pNeighbor || pNeighbor->GetNumPlots() < 7) //ignore very small zones
			continue;

		if (eDomain != NO_DOMAIN && pNeighbor->GetDomain() != eDomain)
			continue;

		int iScore = 0;

		if (pNeighbor->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
			iScore += 8;

		if (pNeighbor->GetTerritoryType() == TACTICAL_TERRITORY_NEUTRAL)
		{
			iScore += 2;

			//similar to GetPlayersAtWarWithInFuture() ...
			if (pNeighbor->GetOwner() != NO_PLAYER && GET_PLAYER(pNeighbor->GetOwner()).isMajorCiv())
			{
				if (GET_PLAYER(m_eOwner).GetDiplomacyAI()->GetVisibleApproachTowardsUs(pNeighbor->GetOwner()) <= MAJOR_CIV_APPROACH_GUARDED)
					iScore++;
				if (GET_PLAYER(m_eOwner).GetDiplomacyAI()->GetMajorCivApproach(pNeighbor->GetOwner()) <= MAJOR_CIV_APPROACH_GUARDED)
					iScore++;
				if (GET_PLAYER(m_eOwner).GetDiplomacyAI()->GetMajorCivOpinion(pNeighbor->GetOwner()) == MAJOR_CIV_OPINION_ENEMY)
					iScore++;
			}
		}

		//try to take into account relative strength
		iScore += (pNeighbor->GetTotalEnemyUnitCount() + pNeighbor->GetNeutralUnitCount()) / max(1,GetTotalFriendlyUnitCount());
		
		if (pNeighbor->GetTerritoryType() == TACTICAL_TERRITORY_NO_OWNER)
			iScore += 1;

		if (iScore > iWorstScore)
		{
			iWorstScore = iScore;
			if (ppWorstNeighborCity)
				*ppWorstNeighborCity = pNeighbor->GetZoneCity();
		}

		iSum += iScore;
	}
	
	return iSum;
}

bool CvTacticalDominanceZone::HasNeighborZone(PlayerTypes eOwner) const
{
	CvTacticalAnalysisMap* pTactMap = GET_PLAYER(m_eOwner).GetTacticalAI()->GetTacticalAnalysisMap();
	for (size_t i = 0; i < m_vNeighboringZones.size(); i++)
	{
		CvTacticalDominanceZone* pNeighbor = pTactMap->GetZoneByID(m_vNeighboringZones[i]);
		if (pNeighbor && pNeighbor->GetOwner() == eOwner)
			return true;
	}

	return false;
}

eTacticalPosture CvTacticalDominanceZone::SelectPostureSingleZone(int iDominancePercent)
{
	m_ePosture = TACTICAL_POSTURE_NONE;

	eTacticalDominanceFlags eOverallDominance = GetOverallDominanceFlag(); //this one is precomputed
	eTacticalDominanceFlags eRangedDominance = IsWater() ? GetNavalRangedDominanceFlag(iDominancePercent) : GetRangedDominanceFlag(iDominancePercent);

	//are our forces mostly ranged and the enemy has a lot of melee units?
	bool bInDangerOfCounterattack = IsWater() ? GetEnemyNavalStrength() > 2 * GetFriendlyNavalStrength() && GetFriendlyNavalRangedStrength() > 2 * GetFriendlyNavalStrength() :
									GetEnemyMeleeStrength() > 2 * GetFriendlyMeleeStrength() && GetFriendlyRangedStrength() > 2 * GetFriendlyMeleeStrength();

	// Choice based on whose territory this is
	switch (GetTerritoryType())
	{
	case TACTICAL_TERRITORY_ENEMY:
	{
		// Default for this zone
		m_ePosture = TACTICAL_POSTURE_ATTRIT_FROM_RANGE;

		// Withdraw if enemy dominant overall or we are vulnerable to counterattacks
		if (eOverallDominance == TACTICAL_DOMINANCE_ENEMY || bInDangerOfCounterattack)
		{
			//try to grab it ...
			CvCity *pClosestCity = GetZoneCity();
			if (pClosestCity && pClosestCity->isInDangerOfFalling())
				m_ePosture = TACTICAL_POSTURE_SURGICAL_CITY_STRIKE;
			else
				m_ePosture = TACTICAL_POSTURE_WITHDRAW;
		}
		else if (eOverallDominance == TACTICAL_DOMINANCE_EVEN)
		{
			if (GetEnemyMeleeStrength() > GetFriendlyMeleeStrength())
			{
				//if we have ranged dominance, keep our risk lower
				m_ePosture = (eRangedDominance == TACTICAL_DOMINANCE_FRIENDLY) ? TACTICAL_POSTURE_ATTRIT_FROM_RANGE : TACTICAL_POSTURE_EXPLOIT_FLANKS;
			}
			else
			{
				//if we have ranged dominance and melee dominance, go all in
				m_ePosture = (eRangedDominance == TACTICAL_DOMINANCE_FRIENDLY) ? TACTICAL_POSTURE_STEAMROLL : TACTICAL_POSTURE_EXPLOIT_FLANKS;
			}
		}
		else if (eOverallDominance == TACTICAL_DOMINANCE_FRIENDLY)
		{
			m_ePosture = TACTICAL_POSTURE_STEAMROLL;
		}
		break;
	}

	case TACTICAL_TERRITORY_NEUTRAL:
	case TACTICAL_TERRITORY_NO_OWNER:
	{
		// Default for this zone
		m_ePosture = (eRangedDominance == TACTICAL_DOMINANCE_FRIENDLY) ? TACTICAL_POSTURE_ATTRIT_FROM_RANGE : TACTICAL_POSTURE_EXPLOIT_FLANKS;

		if (eOverallDominance == TACTICAL_DOMINANCE_ENEMY)
		{
			if (GetEnemyMeleeStrength() > GetFriendlyMeleeStrength())
			{
				//if we have ranged dominance, keep our risk lower
				m_ePosture = (eRangedDominance == TACTICAL_DOMINANCE_FRIENDLY) ? TACTICAL_POSTURE_ATTRIT_FROM_RANGE : TACTICAL_POSTURE_WITHDRAW;
			}
			else
			{
				//if we have ranged dominance, keep our risk lower
				m_ePosture = (eRangedDominance == TACTICAL_DOMINANCE_FRIENDLY) ? TACTICAL_POSTURE_EXPLOIT_FLANKS : TACTICAL_POSTURE_WITHDRAW;
			}
		}

		break;
	}
	case TACTICAL_TERRITORY_FRIENDLY:
	{
		m_ePosture = (eOverallDominance == TACTICAL_DOMINANCE_ENEMY) ? TACTICAL_POSTURE_HEDGEHOG : TACTICAL_POSTURE_COUNTERATTACK;
		break;
	}
	}

	return m_ePosture;
}

//overwrite the posture based on specific conditions in neighbor zones
eTacticalPosture CvTacticalDominanceZone::SelectPostureMultiZone(vector<CvTacticalDominanceZone*> vNeighbors)
{
	for (size_t i = 0; i < vNeighbors.size(); i++)
	{
		//play it safe ...
		if (vNeighbors[i] == NULL)
			continue;

		//if there is a neighboring land zone with a lot of ranged firepower, be careful
		if (IsWater() && GetPosture() == TACTICAL_POSTURE_STEAMROLL)
		{
			if (!vNeighbors[i]->IsWater() &&
				vNeighbors[i]->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_ENEMY &&
				vNeighbors[i]->GetEnemyRangedStrength() > GetFriendlyNavalRangedStrength())
			{
				m_ePosture = TACTICAL_POSTURE_EXPLOIT_FLANKS;
			}
		}

		//withdraw if there is a neighboring zone which needs support 
		if (GetTerritoryType() != TACTICAL_TERRITORY_FRIENDLY)
		{
			if (IsWater() == vNeighbors[i]->IsWater() &&
				vNeighbors[i]->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY &&
				vNeighbors[i]->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_ENEMY)
			{
				CvCity* pCity = vNeighbors[i]->GetZoneCity();
				if (!pCity || !pCity->isInDangerOfFalling())
					m_ePosture = TACTICAL_POSTURE_WITHDRAW;
			}
		}
	}

	return m_ePosture;
}

void CvTacticalDominanceZone::AddNeighboringZone(int iZoneID)
{ 
	if (iZoneID==m_iZoneID || iZoneID==-1)
		return;

	std::vector<int>::iterator it = std::find(m_vNeighboringZones.begin(),m_vNeighboringZones.end(),iZoneID);

	if (it==m_vNeighboringZones.end())
		m_vNeighboringZones.push_back(iZoneID);
}

void CvTacticalAnalysisMap::EstablishZoneNeighborhood()
{
	//walk over the map and see which zones are adjacent
	int iW = GC.getMap().getGridWidth();
	int iH = GC.getMap().getGridHeight();

	for(size_t iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		m_DominanceZones[iI].ClearNeighboringZones();
	}

	for (int i=0; i<iW; i++)
	{
		for (int j=0; j<iH; j++)
		{
			CvPlot* pA = GC.getMap().plot(i,j);
			CvPlot* pB = GC.getMap().plot(i,j+1); //next row
			CvPlot* pC = GC.getMap().plot(i+1,j); //next col

			int iA = pA ? GetDominanceZoneID(pA->GetPlotIndex()) : -1;
			int iB = pB ? GetDominanceZoneID(pB->GetPlotIndex()) : -1;
			int iC = pC ? GetDominanceZoneID(pC->GetPlotIndex()) : -1;

			if (iA!=-1 && iB!=-1)
			{
				if (GetZoneByID(iA)->GetTerritoryType()!=TACTICAL_TERRITORY_NO_OWNER ||
					GetZoneByID(iB)->GetTerritoryType()!=TACTICAL_TERRITORY_NO_OWNER)
				{
					GetZoneByID(iA)->AddNeighboringZone(iB);
					GetZoneByID(iB)->AddNeighboringZone(iA);
				}
			}
			if (iA!=-1 && iC!=-1)
			{
				if (GetZoneByID(iA)->GetTerritoryType()!=TACTICAL_TERRITORY_NO_OWNER ||
					GetZoneByID(iC)->GetTerritoryType()!=TACTICAL_TERRITORY_NO_OWNER)
				{
					GetZoneByID(iA)->AddNeighboringZone(iC);
					GetZoneByID(iC)->AddNeighboringZone(iA);
				}
			}
		}
	}
}

bool CvTacticalAnalysisMap::IsUpToDate()
{
	return (m_iTurnSliceBuilt == GC.getGame().getTurnSlice() && m_vPlotZoneID.size()==GC.getMap().numPlots());
}

void CvTacticalAnalysisMap::Invalidate()
{
	m_iTurnSliceBuilt = -1;
}

/// Fill the map with data for this AI player's turn
void CvTacticalAnalysisMap::Refresh(bool force)
{
	if(force || !IsUpToDate())
	{
		//can happen in the first turn ...
		if (m_vPlotZoneID.size()!=GC.getMap().numPlots())
			Init(m_ePlayer);

		m_iTurnSliceBuilt = GC.getGame().getTurnSlice();
		m_iTacticalRangeTurns = (GC.getAI_TACTICAL_RECRUIT_RANGE() + GC.getGame().getCurrentEra()) / 2;
		m_iUnitStrengthMultiplier = 1; //relative to cities

		AI_PERF_FORMAT("AI-perf.csv", ("Tactical Analysis Map, Turn %d, %s", GC.getGame().getGameTurn(), GET_PLAYER(m_ePlayer).getCivilizationShortDescription()) );

		m_DominanceZones.clear();

		TeamTypes eTeam = GET_PLAYER(m_ePlayer).getTeam();
		for(int iI = 0; iI < GC.getMap().numPlots(); iI++)
		{
			CvPlot* pPlot = GC.getMap().plotByIndexUnchecked(iI);
			if (!pPlot)
				continue;

			if(pPlot->isRevealed(eTeam) && !pPlot->isImpassable(eTeam))
				m_vPlotZoneID[iI] = AddToDominanceZones(iI);
			else
				m_vPlotZoneID[iI] = -1;
		}

		//do this before anything else
		UpdateZoneIds();

		//barbarians don't care about tactical dominance
		if(m_ePlayer!=BARBARIAN_PLAYER)
		{
			EstablishZoneNeighborhood();
			CalculateMilitaryStrengths();
			UpdatePostures();
			PrioritizeZones();

			//only temporary measure, creates a huge amount of logs
			//LogZones();
		}
	}
}

//make it so that the zone ids correspond to the city ids (easier debugging)
void CvTacticalAnalysisMap::UpdateZoneIds()
{
	map<int, int> old2new;

	for (size_t i = 0; i < m_DominanceZones.size(); i++)
	{
		CvCity* pZoneCity = m_DominanceZones[i].GetZoneCity();
		int iOld = m_DominanceZones[i].GetZoneID();
		int iNew = m_DominanceZones[i].GetZoneID();
		if (pZoneCity)
		{
			if (m_DominanceZones[i].IsWater())
				m_DominanceZones[i].SetZoneID(-pZoneCity->GetID());
			else
				m_DominanceZones[i].SetZoneID(+pZoneCity->GetID());

			iNew = m_DominanceZones[i].GetZoneID();
		}
		old2new[iOld] = iNew;
	}

	for (size_t i = 0; i < m_vPlotZoneID.size(); i++)
		if (m_vPlotZoneID[i]!=-1)
			m_vPlotZoneID[i] = old2new[m_vPlotZoneID[i]];

	m_IdLookup.clear();
	for (size_t i = 0; i < m_DominanceZones.size(); i++)
		m_IdLookup[m_DominanceZones[i].GetZoneID()] = i;
}

int CvTacticalAnalysisMap::GetNumZones() 
{ 
	if (!IsUpToDate())
		Refresh();

	return m_DominanceZones.size(); 
};

/// Add data for this cell into dominance zone information
int CvTacticalAnalysisMap::AddToDominanceZones(int iIndex)
{
	TeamTypes ourTeam = GET_PLAYER(m_ePlayer).getTeam();
	CvPlot* pPlot = GC.getMap().plotByIndex(iIndex);

	// Compute zone data for this cell
	CvTacticalDominanceZone newZone;
	newZone.SetAreaID(pPlot->getArea());
	newZone.SetZoneID(pPlot->isWater() ? -pPlot->GetPlotIndex() : pPlot->GetPlotIndex());

	//using estimated turn distance is nice but a performance hog
	int iCityDistance = GC.getGame().GetClosestCityDistanceInPlots(pPlot);
	CvCity* pCity = GC.getGame().GetClosestCityByPlots(pPlot);
	PlayerTypes eOwnerPlayer = NO_PLAYER;
	TeamTypes eOwnerTeam = NO_TEAM;

	//for plots far away from a city, check the owner
	if (iCityDistance>2)
	{
		eOwnerTeam = pPlot->getTeam();
		eOwnerPlayer = pPlot->getOwner();

		//there is almost always a closest city, but we're not always interested
		if (pCity && eOwnerPlayer!=pCity->getOwner())
			pCity = NULL;
	}
	else if (pCity) //look at the city
	{
		eOwnerTeam = pCity->getTeam();
		eOwnerPlayer = pCity->getOwner();

		//for very small areas which can be dominated from the surrounding tiles, use the city's area id instead!
		if (pPlot->area()->getNumTiles()<7)
			newZone.SetAreaID(pCity->getArea());
	}

	newZone.SetOwner(eOwnerPlayer);
	newZone.SetZoneCity(pCity);
	newZone.Extend(pPlot);

	if(eOwnerTeam==NO_TEAM)
	{
		newZone.SetTerritoryType(TACTICAL_TERRITORY_NO_OWNER);
	}
	else if(eOwnerTeam == ourTeam)
	{
		newZone.SetTerritoryType(TACTICAL_TERRITORY_FRIENDLY);
	}
	else if(GET_TEAM(ourTeam).isAtWar(eOwnerTeam))
	{
		newZone.SetTerritoryType(TACTICAL_TERRITORY_ENEMY);
	}
	else
	{
		newZone.SetTerritoryType(TACTICAL_TERRITORY_NEUTRAL);
	}

	// Now see if we already have a matching zone
	CvTacticalDominanceZone* pZone = MergeWithExistingZone(&newZone);
	if (!pZone)
		pZone = AddNewDominanceZone(newZone);

	pZone->Extend(pPlot);
	return pZone->GetZoneID();
}

CvTacticalDominanceZone* CvTacticalAnalysisMap::AddNewDominanceZone(CvTacticalDominanceZone& zone)
{
	m_IdLookup[zone.GetZoneID()] = m_DominanceZones.size();
	m_DominanceZones.push_back(zone);
	return &m_DominanceZones.back();
}

/// Calculate military presences in each owned dominance zone
void CvTacticalAnalysisMap::CalculateMilitaryStrengths()
{
	TeamTypes eTeam = GET_PLAYER(m_ePlayer).getTeam();

	//weigh units close to the center of the zone higher
	int iMaxDistance = GetTacticalRangeTurns();	//turns to plots is factor 2, distance to diameter is also factor 2
	int iBias = 2; // some bias because action may still be spread out over the zone

	// Loop through the dominance zones
	for(unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];
		CvCity *pZoneCity = pZone->GetZoneCity();

		if(pZoneCity)
		{
			// Start with strength of the city itself
			int iCityHitPoints = pZoneCity->GetMaxHitPoints() - pZoneCity->getDamage();
			int iStrength = (pZoneCity->getStrengthValue() * iCityHitPoints) / pZoneCity->GetMaxHitPoints();

			if(pZone->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY)
				pZone->AddFriendlyRangedStrength(iStrength);
			else if(pZone->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
				pZone->AddEnemyRangedStrength(iStrength);
			else
				pZone->AddNeutralStrength(iStrength);
		}

		// check all units in the world
		for(int iPlayerLoop = 0; iPlayerLoop < MAX_PLAYERS; iPlayerLoop++)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes) iPlayerLoop);
			bool bEnemy = GET_TEAM(eTeam).isAtWar(kPlayer.getTeam());
			bool bFriendly = (eTeam==kPlayer.getTeam());

			int iLoop;
			for(CvUnit* pLoopUnit = kPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iLoop))
			{
				if(!pLoopUnit->IsCombatUnit())
					continue;

				CvPlot* pPlot = pLoopUnit->plot();
				if(!pPlot)
					continue;

				bool bZoneTypeMatch = 
							(pLoopUnit->getDomainType() == DOMAIN_AIR) ||
							(pLoopUnit->getDomainType() == DOMAIN_LAND && !pZone->IsWater()) || 
							(pLoopUnit->getDomainType() == DOMAIN_SEA && pZone->IsWater());

				if (!bZoneTypeMatch && !pLoopUnit->isRanged())
					continue;

				//a little cheating for AI - invisible units still count with reduced strength
				bool bVisible = pPlot->isVisible(eTeam) || pPlot->isAdjacentVisible(eTeam, false);
				bool bReducedStrength = pLoopUnit->isEmbarked() || !bVisible || !bZoneTypeMatch;

				int iPlotDistance = 0;
				//if there is a city, units in adjacent zones can also count
				if (pZoneCity)
				{
					iPlotDistance = plotDistance(*pPlot, *pZoneCity->plot());
					if (iPlotDistance>iMaxDistance)
						continue; //unit is too far away
				}
				else
				{
					//if there is no city, the unit must be in the zone itself, distance is zero by definition
					if ( GetDominanceZoneID(pLoopUnit->plot()->GetPlotIndex()) != pZone->GetZoneID() )
						continue;
				}

				int iEffectiveDistance = MAX(0,iPlotDistance - iBias); 
				int iMultiplier = MAX(1,iMaxDistance - iEffectiveDistance);  
				int iUnitStrength = pLoopUnit->GetMaxAttackStrength(NULL,NULL,NULL,true,true) * iMultiplier * m_iUnitStrengthMultiplier;
				int iRangedStrength = pLoopUnit->GetMaxRangedCombatStrength(NULL, /*pCity*/ NULL, true, NULL, NULL, true, true) * iMultiplier * m_iUnitStrengthMultiplier;

				if(bReducedStrength)
				{
					iUnitStrength /= 2;
					iRangedStrength /= 2;
				}

				if (bEnemy)
				{
					if (pLoopUnit->getDomainType() == DOMAIN_SEA)
					{
						pZone->AddEnemyNavalStrength(iUnitStrength);
						pZone->AddEnemyNavalRangedStrength(iRangedStrength);
						pZone->AddEnemyNavalUnitCount(1);
					}
					else
					{
						pZone->AddEnemyMeleeStrength(iUnitStrength);
						pZone->AddEnemyRangedStrength(iRangedStrength);
						pZone->AddEnemyUnitCount(1);
					}

					//again only for enemies
					if(pZone->GetDistanceOfClosestEnemyUnit()<0 || iPlotDistance<pZone->GetDistanceOfClosestEnemyUnit())
						pZone->SetDistanceOfClosestEnemyUnit(iPlotDistance);
				}
				else if (bFriendly)
				{

					if (pLoopUnit->getDomainType() == DOMAIN_SEA)
					{
						pZone->AddFriendlyNavalStrength(iUnitStrength);
						pZone->AddFriendlyNavalRangedStrength(iRangedStrength);
						pZone->AddFriendlyNavalUnitCount(1);
					}
					else
					{
						pZone->AddFriendlyMeleeStrength(iUnitStrength);
						pZone->AddFriendlyRangedStrength(iRangedStrength);
						pZone->AddFriendlyUnitCount(1);
					}
				}
				else
				{
					//neutral has only very few stats
					pZone->AddNeutralStrength(iUnitStrength);
					pZone->AddNeutralUnitCount(1);
				}
			}
		}
	}
}

/// Establish order of zone processing for the turn
void CvTacticalAnalysisMap::PrioritizeZones()
{
	int iMostValuableCity = 1;
	for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvCity* pZoneCity = m_DominanceZones[iI].GetZoneCity();
		if (pZoneCity)
			iMostValuableCity = max(iMostValuableCity, pZoneCity->getEconomicValue(m_ePlayer));
	}

	// Loop through the dominance zones
	for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		// Find the zone and compute dominance here
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];
		eTacticalDominanceFlags eDominance = ComputeDominance(pZone);

		// Establish a base value for the region
		int iBaseValue = 1;
		int iMultiplier = 1;

		CvCity* pZoneCity = pZone->GetZoneCity();
		if (pZoneCity && pZoneCity->isAdjacentToArea(pZone->GetAreaID()))
		{
			//should we take into account distance to the border as well? probably dominance is enough
			iBaseValue += (int)sqrt(pZoneCity->getEconomicValue(m_ePlayer)*100.f/iMostValuableCity);

			if (GET_PLAYER(m_ePlayer).GetTacticalAI()->IsInFocusArea(pZoneCity->plot()))
			{
				iBaseValue *= 3;
			}

			if (pZoneCity->isVisible(GET_PLAYER(m_ePlayer).getTeam(), false))
			{
				iBaseValue *= 2;

				// How damaged is this visible city?
				int iMaxDamageMultiplier = 10;
				int iDamage = pZoneCity->getDamage();
				if (iDamage > (pZoneCity->GetMaxHitPoints() / iMaxDamageMultiplier))
				{
					iBaseValue *= (int)((iDamage + 1) * 10 / pZoneCity->GetMaxHitPoints());
				}
			}

#if defined(MOD_BALANCE_CORE)
			if (GET_PLAYER(m_ePlayer).IsTargetCityForOperation(pZoneCity,false) ||
				GET_PLAYER(m_ePlayer).IsTargetCityForOperation(pZoneCity,true) ||
				GET_PLAYER(m_ePlayer).GetMilitaryAI()->IsCurrentAttackTarget(pZoneCity))
			{
				iBaseValue *= 2;
			}

			if (pZoneCity->GetPlayer()->isMinorCiv())
			{
				//At war with ally of this minor? Greatly reduce priority.
				PlayerTypes eAlly = pZoneCity->GetPlayer()->GetMinorCivAI()->GetAlly();
				if (eAlly != NO_PLAYER && GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).isAtWar(GET_PLAYER(eAlly).getTeam()))
				{
					iBaseValue = 1;
				}
			}
#endif
		}

		if (!pZone->IsWater())
		{
			iBaseValue *= 3;
		}

		// Now compute a multiplier based on current conditions here
		if (eDominance == TACTICAL_DOMINANCE_ENEMY)
		{
			if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
			{
				iMultiplier = 1;
			}
			else if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY)
			{
				iMultiplier = 8;
			}
		}
		else if (eDominance == TACTICAL_DOMINANCE_EVEN && pZone->GetTotalEnemyUnitCount()>1) //make sure there's an actual fight going on
		{
			if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
			{
				iMultiplier = 4;
			}
			else if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY)
			{
				iMultiplier = 4;
			}
		}
		else if (eDominance == TACTICAL_DOMINANCE_FRIENDLY)
		{
			if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
			{
				iMultiplier = 8;
			}
			else if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY)
			{
				iMultiplier = 1;
			}
		}
		if (!GET_PLAYER(m_ePlayer).isMinorCiv())
		{
			if (GET_PLAYER(m_ePlayer).GetDiplomacyAI()->GetStateAllWars() == STATE_ALL_WARS_WINNING)
			{
				if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_ENEMY)
				{
					iMultiplier *= 4;
				}
			}
			else if (GET_PLAYER(m_ePlayer).GetDiplomacyAI()->GetStateAllWars() == STATE_ALL_WARS_LOSING)
			{
				if (pZone->GetTerritoryType() == TACTICAL_TERRITORY_FRIENDLY)
				{
					iMultiplier *= 4;
				}
			}
		}

		// Save off the value for this zone
		pZone->SetDominanceZoneValue(iBaseValue * iMultiplier);
	}

	std::stable_sort(m_DominanceZones.begin(), m_DominanceZones.end());

	m_IdLookup.clear();
	for (size_t i = 0; i < m_DominanceZones.size(); i++)
		m_IdLookup[m_DominanceZones[i].GetZoneID()] = i;
}

/// Establish postures for each dominance zone (taking into account last posture)
void CvTacticalAnalysisMap::UpdatePostures()
{
	// first pass, look at zones in isolation
	for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];
		//todo: should we include the previous posture in the logic?
		//but we've thrown it away at this point ...
		pZone->SelectPostureSingleZone(GetDominancePercentage());
	}

	// second pass, look at neighbors as well
	// note that we don't check for convergence ...
	for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];
		//need to look up the neighbors manually ...
		vector<CvTacticalDominanceZone*> vNeighbors;
		vector<int> vNeighborIds = pZone->GetNeighboringZones();
		for (size_t j = 0; j < vNeighborIds.size(); j++)
			vNeighbors.push_back( GetZoneByID(vNeighborIds[j]) );

		//todo: should we include the previous posture in the logic?
		//but we've thrown it away at this point ...
		pZone->SelectPostureMultiZone(vNeighbors);
	}

	//third pass, logging only
	for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];
		if(GC.getLogging() && GC.getAILogging())
		{
			CvString szPostureMsg;
			szPostureMsg.Format("Zone ID: %d, %s, %s, ", pZone->GetZoneID(), pZone->IsWater() ? "Water" : "Land", pZone->GetZoneCity() ? pZone->GetZoneCity()->getName().c_str() : "none");

			switch(pZone->GetPosture())
			{
			case TACTICAL_POSTURE_ATTRIT_FROM_RANGE:
				szPostureMsg += "Attrit from Range";
				break;
			case TACTICAL_POSTURE_EXPLOIT_FLANKS:
				szPostureMsg += "Exploit Flanks";
				break;
			case TACTICAL_POSTURE_STEAMROLL:
				szPostureMsg += "Steamroll";
				break;
			case TACTICAL_POSTURE_SURGICAL_CITY_STRIKE:
				szPostureMsg += "Surgical City Strike";
				break;
			case TACTICAL_POSTURE_HEDGEHOG:
				szPostureMsg += "Hedgehog";
				break;
			case TACTICAL_POSTURE_COUNTERATTACK:
				szPostureMsg += "Counterattack";
				break;
			case TACTICAL_POSTURE_WITHDRAW:
				szPostureMsg += "Withdraw";
				break;
			case TACTICAL_POSTURE_NONE:
				szPostureMsg += "NoPosture";
				break;
			}
			GET_PLAYER(m_ePlayer).GetTacticalAI()->LogTacticalMessage(szPostureMsg);
		}
	}
}

/// Log dominance zone data
void CvTacticalAnalysisMap::LogZones()
{
	if (GC.getLogging() && GC.getAILogging() && GET_PLAYER(m_ePlayer).isMajorCiv())
	{
		std::stringstream ss;
		ss << "c:\\temp\\DominanceZones_" << GET_PLAYER(m_ePlayer).getCivilizationAdjective() << "_" << std::setfill('0') << std::setw(3) << GC.getGame().getGameTurn() << ".txt";
		std::ofstream of(ss.str().c_str());
		if (of.good())
		{
			of << "#x,y,terrain,owner,zoneid\n";
			for (size_t i = 0; i < m_vPlotZoneID.size(); i++)
			{
				CvPlot* pPlot = GC.getMap().plotByIndex(i);
				if (pPlot->isRevealed(GET_PLAYER(m_ePlayer).getTeam()))
				{
					CvString dump = CvString::format("%d,%d,%d,%d,%d\n", pPlot->getX(), pPlot->getY(), pPlot->getTerrainType(), pPlot->getOwner(), m_vPlotZoneID[i]);
					of << dump.c_str();
				}
			}

			of << "#------------------\n";

			for (unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
			{
				CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];

				//don't blow up the logs for empty zones
				if (pZone->GetOverallFriendlyStrength() == 0 && pZone->GetOverallEnemyStrength() == 0)
					continue;

				CvString szLogMsg;
				szLogMsg.Format("Zone ID: %d, %s, Size: %d, City: %s, Area ID: %d, Value: %d, FRIENDLY Str: %d (%d), Ranged: %d (naval %d), ENEMY Str: %d (%d), Ranged: %d (naval %d), Closest Enemy: %d",
					pZone->GetZoneID(), pZone->IsWater() ? "Water" : "Land", pZone->GetNumPlots(), pZone->GetZoneCity() ? pZone->GetZoneCity()->getName().c_str() : "none", pZone->GetAreaID(), pZone->GetDominanceZoneValue(),
					pZone->GetOverallFriendlyStrength(), pZone->GetTotalFriendlyUnitCount(), pZone->GetFriendlyRangedStrength(), pZone->GetFriendlyNavalRangedStrength(),
					pZone->GetOverallEnemyStrength(), pZone->GetTotalEnemyUnitCount(), pZone->GetEnemyRangedStrength(), pZone->GetEnemyNavalRangedStrength(), pZone->GetDistanceOfClosestEnemyUnit());

				if (pZone->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_FRIENDLY)
				{
					szLogMsg += ", Friendly";
				}
				else if (pZone->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_ENEMY)
				{
					szLogMsg += ", Enemy";
				}
				else if (pZone->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_EVEN)
				{
					szLogMsg += ", Even";
				}
				else if (pZone->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_NO_UNITS_VISIBLE)
				{
					szLogMsg += ", No Units Visible";
				}

				if (pZone->IsWater())
				{
					szLogMsg += ", Water";
				}
				else
				{
					szLogMsg += ", Land";
				}

				if (pZone->GetZoneCity())
				{
					if (GET_PLAYER(m_ePlayer).GetTacticalAI()->IsInFocusArea(pZone->GetZoneCity()->plot()))
					{
						szLogMsg += " (Focus)";
					}
				}

				of << szLogMsg.c_str() << "\n";
			}

			of.close();
		}
	}
}

/// Can this cell go in an existing dominance zone?
CvTacticalDominanceZone* CvTacticalAnalysisMap::MergeWithExistingZone(CvTacticalDominanceZone* pNewZone)
{
	for(unsigned int iI = 0; iI < m_DominanceZones.size(); iI++)
	{
		CvTacticalDominanceZone* pZone = &m_DominanceZones[iI];

		// Keep the elements nicely separated
		if (pZone->IsWater() != pNewZone->IsWater())
			continue;

		// If this is a temporary zone, matches if unowned and close enough
		int iDistance = plotDistance(pNewZone->GetCenterX(), pNewZone->GetCenterY(), pZone->GetCenterX(), pZone->GetCenterY());
		if((pNewZone->GetTerritoryType() == TACTICAL_TERRITORY_NO_OWNER || pNewZone->GetTerritoryType() == TACTICAL_TERRITORY_NEUTRAL) &&
		        (iDistance <= GetTacticalRangeTurns())) //awkward: turns vs plots
		{
			return pZone;
		}

		// If unowned, just 1 zone per area
		if((pZone->GetTerritoryType() == TACTICAL_TERRITORY_NO_OWNER) &&
		        (pNewZone->GetTerritoryType() == TACTICAL_TERRITORY_NO_OWNER) &&
				(pZone->GetAreaID() == pNewZone->GetAreaID()))
		{
			return pZone;
		}

		// Otherwise everything needs to match
		if(pZone->GetTerritoryType() == pNewZone->GetTerritoryType() &&
		        pZone->GetOwner() == pNewZone->GetOwner() &&
		        pZone->GetAreaID() == pNewZone->GetAreaID() && //do not check water flag
		        pZone->GetZoneCity() == pNewZone->GetZoneCity())
		{
			return pZone;
		}
	}

	return NULL;
}

/// Retrieve a dominance zone
CvTacticalDominanceZone* CvTacticalAnalysisMap::GetZoneByIndex(int iIndex)
{
	if (!IsUpToDate())
		Refresh();

	if(iIndex < 0 || iIndex >= (int)m_DominanceZones.size())
		return NULL;
	return &m_DominanceZones[iIndex];
}

/// Retrieve a dominance zone by closest city
CvTacticalDominanceZone* CvTacticalAnalysisMap::GetZoneByCity(const CvCity* pCity, bool bWater)
{
	if (!IsUpToDate())
		Refresh();

	if (!pCity)
		return NULL;

	//water zones have negative ids
	return GetZoneByID(pCity->GetID()*(bWater ? -1 : +1));
}

/// Retrieve a dominance zone by ID
CvTacticalDominanceZone* CvTacticalAnalysisMap::GetZoneByID(int iID)
{
	if (!IsUpToDate())
		Refresh();

	map<int, int>::iterator it = m_IdLookup.find(iID);
	if (it != m_IdLookup.end())
		return GetZoneByIndex(it->second);

	return NULL;
}

CvTacticalDominanceZone * CvTacticalAnalysisMap::GetZoneByPlot(const CvPlot * pPlot)
{
	if (!IsUpToDate())
		Refresh();

	if (!pPlot || pPlot->GetPlotIndex()>=(int)m_vPlotZoneID.size())
		return NULL;

	return GetZoneByID(m_vPlotZoneID[pPlot->GetPlotIndex()]);
}

int CvTacticalAnalysisMap::GetDominanceZoneID(int iPlotIndex)
{
	if (!IsUpToDate())
		Refresh();

	if (iPlotIndex<0 || iPlotIndex>=(int)m_vPlotZoneID.size())
		return NULL;

	return m_vPlotZoneID[iPlotIndex];
}

// Is this plot in dangerous territory?
bool CvTacticalAnalysisMap::IsInEnemyDominatedZone(const CvPlot* pPlot)
{
	if (!IsUpToDate())
		Refresh();

	if (!pPlot || pPlot->GetPlotIndex()>=(int)m_vPlotZoneID.size())
		return NULL;

	CvTacticalDominanceZone* pZone = GetZoneByID(m_vPlotZoneID[pPlot->GetPlotIndex()]);

	if(pZone && pZone->GetZoneCity()) //city check is to skip the potentially very large ocean zone
		return (pZone->GetOverallDominanceFlag() == TACTICAL_DOMINANCE_ENEMY);

	return false;
}

/// Who is dominant in this one zone?
eTacticalDominanceFlags CvTacticalAnalysisMap::ComputeDominance(CvTacticalDominanceZone* pZone)
{
	// Look at ratio of friendly to enemy strength
	if (!pZone)
		return TACTICAL_DOMINANCE_NO_UNITS_VISIBLE;

	if(pZone->GetOverallEnemyStrength()+pZone->GetOverallFriendlyStrength()<=0)
	{
		pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_NO_UNITS_VISIBLE);
	}
	else
	{
		// Otherwise compute it by strength
		if (pZone->GetOverallEnemyStrength() <= 0 && pZone->GetTotalFriendlyUnitCount() > 0) //make sure the denominator is valid later on
		{
			pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_FRIENDLY);
		}
		else if (pZone->GetTotalEnemyUnitCount()==1 && pZone->GetTotalFriendlyUnitCount()<=1) //both are weak, one unit might tip the balance
		{
			pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_EVEN);
		}
		else
		{
			//a bit complex to make sure there is no overflow
			int iRatio = int(0.5f + 100 * ( float(pZone->GetOverallFriendlyStrength()) / max(1u, pZone->GetOverallEnemyStrength())));
			if (iRatio > 100 + m_iDominancePercentage)
			{
				pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_FRIENDLY);
			}
			else if (iRatio < 100 - m_iDominancePercentage)
			{
				pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_ENEMY);
			}
			else
			{
				pZone->SetOverallDominanceFlag(TACTICAL_DOMINANCE_EVEN);
			}
		}
	}

	return pZone->GetOverallDominanceFlag();
}

FDataStream& operator<<(FDataStream& saveTo, const CvTacticalAnalysisMap& readFrom)
{
	saveTo << readFrom.m_iDominancePercentage;
	saveTo << readFrom.m_iUnitStrengthMultiplier;
	saveTo << readFrom.m_iTacticalRangeTurns;
	saveTo << readFrom.m_ePlayer;
	saveTo << readFrom.m_iTurnSliceBuilt;

	saveTo << readFrom.m_vPlotZoneID;
	saveTo << readFrom.m_DominanceZones;

	return saveTo;
}
FDataStream& operator>>(FDataStream& loadFrom, CvTacticalAnalysisMap& writeTo)
{
	loadFrom >> writeTo.m_iDominancePercentage;
	loadFrom >> writeTo.m_iUnitStrengthMultiplier;
	loadFrom >> writeTo.m_iTacticalRangeTurns;
	loadFrom >> writeTo.m_ePlayer;
	loadFrom >> writeTo.m_iTurnSliceBuilt;

	loadFrom >> writeTo.m_vPlotZoneID;
	loadFrom >> writeTo.m_DominanceZones;

	return loadFrom;
}

FDataStream& operator<<(FDataStream& saveTo, const CvTacticalDominanceZone& readFrom)
{
	saveTo << readFrom.m_iZoneID;
	saveTo << readFrom.m_eTerritoryType;
	saveTo << readFrom.m_eOverallDominanceFlag;
	saveTo << readFrom.m_ePosture;
	saveTo << readFrom.m_eOwner;
	saveTo << readFrom.m_iCityID;
	saveTo << readFrom.m_iAreaID;
	saveTo << readFrom.m_iFriendlyMeleeStrength;
	saveTo << readFrom.m_iEnemyMeleeStrength;
	saveTo << readFrom.m_iFriendlyRangedStrength;
	saveTo << readFrom.m_iEnemyRangedStrength;
	saveTo << readFrom.m_iFriendlyNavalStrength;
	saveTo << readFrom.m_iEnemyNavalStrength;
	saveTo << readFrom.m_iFriendlyNavalRangedStrength;
	saveTo << readFrom.m_iEnemyNavalRangedStrength;
	saveTo << readFrom.m_iFriendlyUnitCount;
	saveTo << readFrom.m_iEnemyUnitCount;
	saveTo << readFrom.m_iNeutralUnitCount;
	saveTo << readFrom.m_iNeutralUnitStrength;
	saveTo << readFrom.m_iEnemyNavalUnitCount;
	saveTo << readFrom.m_iFriendlyNavalUnitCount;
	saveTo << readFrom.m_iZoneValue;
	saveTo << readFrom.m_iDistanceOfClosestEnemyUnit;
	saveTo << readFrom.m_iAvgX;
	saveTo << readFrom.m_iAvgY;
	saveTo << readFrom.m_iPlotCount;

	saveTo << readFrom.m_vNeighboringZones;

	return saveTo;
}
FDataStream& operator>>(FDataStream& loadFrom, CvTacticalDominanceZone& writeTo)
{
	int tmp;

	loadFrom >> writeTo.m_iZoneID;
	loadFrom >> tmp; writeTo.m_eTerritoryType = (eDominanceTerritoryTypes)tmp;
	loadFrom >> tmp; writeTo.m_eOverallDominanceFlag = (eTacticalDominanceFlags)tmp;
	loadFrom >> tmp; writeTo.m_ePosture = (eTacticalPosture)tmp;
	loadFrom >> writeTo.m_eOwner;
	loadFrom >> writeTo.m_iCityID;
	loadFrom >> writeTo.m_iAreaID;
	loadFrom >> writeTo.m_iFriendlyMeleeStrength;
	loadFrom >> writeTo.m_iEnemyMeleeStrength;
	loadFrom >> writeTo.m_iFriendlyRangedStrength;
	loadFrom >> writeTo.m_iEnemyRangedStrength;
	loadFrom >> writeTo.m_iFriendlyNavalStrength;
	loadFrom >> writeTo.m_iEnemyNavalStrength;
	loadFrom >> writeTo.m_iFriendlyNavalRangedStrength;
	loadFrom >> writeTo.m_iEnemyNavalRangedStrength;
	loadFrom >> writeTo.m_iFriendlyUnitCount;
	loadFrom >> writeTo.m_iEnemyUnitCount;
	loadFrom >> writeTo.m_iNeutralUnitCount;
	loadFrom >> writeTo.m_iNeutralUnitStrength;
	loadFrom >> writeTo.m_iEnemyNavalUnitCount;
	loadFrom >> writeTo.m_iFriendlyNavalUnitCount;
	loadFrom >> writeTo.m_iZoneValue;
	loadFrom >> writeTo.m_iDistanceOfClosestEnemyUnit;
	loadFrom >> writeTo.m_iAvgX;
	loadFrom >> writeTo.m_iAvgY;
	loadFrom >> writeTo.m_iPlotCount;

	loadFrom >> writeTo.m_vNeighboringZones;

	return loadFrom;
}

