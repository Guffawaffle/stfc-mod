-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "c5c4ba911f16f3376e828761714d72a26ebe2a96",
        archive_sha256 = "f120590499bb51844c1a2cd274f5bd58dbae0626b72c2d9f800041fabd08fd63"
    }
end
