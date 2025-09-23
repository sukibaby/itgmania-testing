/**
 * @file MeasureInfo.h
 * @brief Header file defining the MeasureInfo struct for song measure
 *        statistics.
 *
 * This header defines the MeasureInfo struct, which stores notes per
 * second (NPS), peak NPS, and note counts per measure for songs.
 *
 * The corresponding source file (MeasureInfo.cpp) implements calculation
 * from NoteData, string serialization, and Lua bindings.
 *
 * Significant dependencies:
 * - GameConstantsAndTypes.h: Supplies game enums and types.
 * - NoteData.h: Provides NoteData for measure analysis.
 */

#ifndef MEASURE_INFO_H
#define MEASURE_INFO_H

#include "GameConstantsAndTypes.h"
#include "NoteData.h"

struct MeasureInfo
{
	int measureCount;
	float peakNps;
	std::vector<float> npsPerMeasure;
	std::vector<int> notesPerMeasure;

	MeasureInfo()
	{
		Zero();
	}
	
	void Zero()
	{
		measureCount = 0;
		peakNps = 0;
		npsPerMeasure.clear();
		notesPerMeasure.clear();
	}

	RString ToString() const;
	void FromString(const RString& sValues );
	static void CalculateMeasureInfo(const NoteData &in, MeasureInfo &out);
};

#endif
