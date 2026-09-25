#pragma once

// Copy this file to `include/cloud_config.h` and fill in local worker settings.
// The real `cloud_config.h` must stay out of git.

namespace decaflash::secrets {

// Set this to the full Cloudflare Worker chattie endpoint.
static constexpr char kCloudChattieUrl[] = "https://example.workers.dev/api/chattie";

// Use the same shared secret value that the worker stores as MAINFRAME_SHARED_SECRET.
static constexpr char kMainframeSharedSecret[] = "replace-with-mainframe-shared-secret";

}  // namespace decaflash::secrets
