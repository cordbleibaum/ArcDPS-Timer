#pragma once

#include <string>
#include <map>

class Translation {
public:
	Translation();
	const std::string& get(std::string name) const;

private:
	std::map<std::string, std::string> translations = {
		// timer.cpp
		{"TimeFormatterInvalid", "INVALID"},
		{"MarkerStopped", "S"},
		{"MarkerPrepared", "P"},
		{"MarkerRunning", "R"},
		{"ButtonPrepare", "Prepare"},
		{"ButtonStart", "Start"},
		{"TextStop", "Stop"},
		{"TextReset", "Reset"},
		{"TextOutOfDate", "OFFLINE, ADDON OUT OF DATE"},
		{"HeaderSegments", "Segments"},
		{"ButtonSegment", "Segment"},
		{"ButtonClearSegments", "Clear"},
		{"HeaderNumColumn", "#"},
		{"HeaderLastColumn", "Last Time (Duration)"},
		{"HeaderBestColumn", "Best Time (Duration)"},

		// settings.cpp
		{"InputUseCustomID", "Use Custom Group ID"},
		{"InputCustomID", "Custom ID"},
		{"InputOnlyInstance", "Disable outside Instanced Content"},
		{"InputAutoprepare", "Auto Prepare"},
		{"InputAutostop", "Auto Stop"},
		{"InputEarlyGG", "Early /gg threshold"},
		{"InputTimeFormatter", "Time formatter"},
		{"InputHideButtons", "Hide buttons"},
		{"InputStartKey", "Start Key"},
		{"InputStopKey", "Stop Key"},
		{"InputPrepareKey", "Prepare Key"},
		{"InputResetKey", "Reset Key"},
		{"InputSegmentKey", "Segment Key"},
		{"TextKeyNotSet", "(not set)"},
		{"WindowOptionTimer", "Timer"},
		{"WindowOptionSegments", "Timer Segments"},
		{"TooltipAutoPrepare", "Tries to automatically set the timer to prepared,\nand start on movement / skillcast.Still has a few limitations"},
		{"TooltipAutoStop", "Tries to automatically stop the timer. Still experimental"},
		{"TooltipEarlyGG", "Seconds threshold for min duration of a boss kill, \neverything lower gets ignored for auto stop"},
		{"TooltipTimerFormatter", "Format for timer time, see https://en.cppreference.com/w/cpp/chrono/duration/formatter"},
		{"InputUnifiedWindow", "Merge timer and segments window"},
		{"WindowOptionUnified", "Timer+Segments"},

		// trigger_region.cpp
		{"WindowOptionTriggerEditor", "=> Editor"},
		{"HeaderTriggerEditor", "Auto Segment Editor"},
		{"TextPlayerPosition", "Player position: "},
		{"TextAreaSphere", "Sphere trigger"},
		{"TextInputRadius", "Radius"},
		{"InputXYZ", "X/Y/Z"},
		{"ButtonPlaceSphere", "Place Sphere"},
		{"TextAreaPlane", "Plane trigger"},
		{"InputThickness", "Thickness"},
		{"InputXY1", "X/Y 1"},
		{"InputXY2", "X/Y 2"},
		{"InputZ", "Z"},
		{"InputHeight", "Height"},
		{"ButtonPlacePlane", "Place Plane"},
		{"HeaderTypeColumn", "Type"},
		{"HeaderMiddleColumn", "Middle (X/Y/Z)"},
		{"ButtonDelete", "Delete"},
		{"TypeSphere", "Sphere"},
		{"TypePlane", "Plane"},
		{"ButtonSet", "Set"},
		{"HeaderIsInRangeColumn", "In Range?"},
		{"TextYes", "Yes"},
		{"HeaderIsTriggeredColumn", "Is Triggered?"}
	};
};
