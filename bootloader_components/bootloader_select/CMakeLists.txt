idf_component_register(
    SRCS "bootloader_select.c"
    REQUIRES hal bootloader_support
)

idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=bootloader_utility_get_selected_boot_partition" APPEND)
idf_build_set_property(LINK_OPTIONS "-Wl,--require-defined=__wrap_bootloader_utility_get_selected_boot_partition" APPEND)
idf_build_set_property(LINK_OPTIONS "-Wl,--require-defined=bootloader_select_include" APPEND)
