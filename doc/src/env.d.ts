/// <reference path="../.astro/types.d.ts" />
/// <reference types="astro/client" />

interface ImportMetaEnv {
  /**
   * Customizable Deploy URL.
   *
   * @default "https://docs.cataclysmbn.org"
   */
  readonly CUSTOM_SITE_URL?: string

  /**
   * Customizable Repository URL for Edit Link and GitHub Social Link.
   *
   * @default "https://github.com/cataclysmbn/Cataclysm-BN"
   */
  readonly CUSTOM_REPO_URL?: string
}
