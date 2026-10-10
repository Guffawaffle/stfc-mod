-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "1cd7a9ccdba5df872b95b9342c0eb8ef8a7cb6eb",
        archive_sha256 = "e1d7f74728c9072e9ac9ddb2f336cc72ef512933eddcea70b0399027541febe2"
    }
end
