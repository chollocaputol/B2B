// Copy privately to study_sf_cloud_config.generated.h and configure.
// This example is intentionally disabled. Use the enrolled node and its own login.
// Never commit the generated header or a credential-bearing image.
const StudyCloudConfig cloudConfig{
    0,          // Enrolled node number
    nullptr,    // Wi-Fi SSID
    nullptr,    // Wi-Fi password (use "" for an open network)
    nullptr,    // Device account email
    nullptr,    // Device account password
    nullptr     // Supabase publishable key (sb_publishable_...)
};
