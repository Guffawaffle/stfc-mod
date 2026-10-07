-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "a7b0929ff5a6245ffe44ea217d181dc512ecb98a",
        archive_sha256 = "a5e1648e772388884141121ee35a6259c6528e62434ac480d9af3fa22270a118"
    }
end
