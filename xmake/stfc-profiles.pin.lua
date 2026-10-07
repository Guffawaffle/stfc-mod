-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "f6a67f9b617ba190a1200dfa3e8a80b6226ebfa4",
        archive_sha256 = "3a76abe854ab618a6c7d48e596eb6a6fd3133a4e69f2cfc5d58bbe667a91c92b"
    }
end
