-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "6b15a352c445efb817634e8ef6be3de4d40818f8",
        archive_sha256 = "7ac8a6d6494f766b287c1ff031ce4348290a619b34ca353caf52b50a7c4488c4"
    }
end
