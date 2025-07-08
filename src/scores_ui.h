#pragma once

class achievements_tracker;
class kill_tracker;
class stats_tracker;

/** Display achievements, stats and kills in a scrollable window, each in their own tab */
void show_scores_ui( const achievements_tracker &achievements, stats_tracker &,
                     const kill_tracker & );
/** Display kills in a scrollable window */
void show_kills( kill_tracker & );


