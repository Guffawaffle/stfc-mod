-- Immutable shared-source lock. Update only after the profile source is reviewed and published.
function stfc_profiles_pin()
    return {
        repository = "https://github.com/Guffawaffle/stfc-profiles.git",
        revision = "07ca529f5a31b20d93648166d7451fd926a741b0",
        archive_sha256 = "377642c37a79d4429aa8b504e8db7ec2850abb4829cb0927e20dae29778b727d"
    }
end
