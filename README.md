Comparing Acceleration Structures Applied to Photon Mapping
===============

This is implementation of my bachelor thesis "Comparing Acceleration Structures Applied to Photon Mapping" using pbrt-v3 renderer.

To utilize the acceleration structures use these integrator names in the scenes files (.pbrt):

"grid_sppm"				for sequential hash grid
"grid_par_sppm"				for parallel hash grid
"nested_grid_sppm"			for sequential nested grid
"nested_grid_par_sppm"			for parallel nested grid
"octree_sppm"				for sequential octree
"octree_par_sppm"			for parallel octree
"sah_inplace_kd_par_sppm"		for parallel kd tree with SAH
"sah_nested_kd_sppm"			for sequential kd tree with SAH
"sah_nested_kd_parsort_sppm"		for kd tree with SAH and parallel sorting
"splitmiddle_nested_kd_sppm"		for sequential kd tree with median splitting
"bvh_sppm"				for parallel bvh

