add_library(variant-openamp variant.c)

target_link_libraries(variant-openamp
	PUBLIC variant-header
	PRIVATE openamp_support
	)
