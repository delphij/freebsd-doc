# GEMINI.MD: AI Collaboration Guide

This document provides essential context for AI models interacting with this project. Adhering to these guidelines will ensure consistency and maintain code quality.

## 1. Project Overview & Purpose

* **Primary Goal:** This project contains the source code for the official FreeBSD documentation and website. Its primary purpose is to build and maintain the documentation sets and web pages for the FreeBSD operating system. The project is structured into two main components: `documentation` and `website`.
* **Business Domain:** Operating System Documentation and Technical Writing.

## 2. Core Technologies & Stack

* **Languages:** AsciiDoc (`.adoc`) is the primary language for documentation content. Shell Script (`.sh`), Ruby (`.rb`), and Makefiles (`Makefile`) are used for build automation and tooling.
* **Frameworks & Runtimes:**
    * **Hugo:** The static site generator used to build the documentation (`/documentation`) and likely the website (`/website`). This is confirmed by the presence of `hugo.toml` and other Hugo configuration files.
    * **Asciidoctor:** Used to process AsciiDoc files, as indicated by `asciidoctor.sh` and custom Ruby-based extensions in `shared/lib`.
* **Databases:** None detected. The project builds a static website.
* **Key Libraries/Dependencies:** No formal package manager is present. Dependencies are likely managed as part of the FreeBSD development environment, as suggested by the `.cirrus.yml` configuration.
* **Package Manager(s):** None detected.

## 3. Architectural Patterns

* **Overall Architecture:** Static Site Generation. The project takes source content in AsciiDoc format, processes it with Hugo and Asciidoctor, and generates a static HTML website.
* **Directory Structure Philosophy:**
    * `/documentation`: Contains the source for the main FreeBSD documentation (books, articles, etc.). It is a self-contained Hugo project.
    * `/website`: Contains the source for the main FreeBSD website. It is also structured as a Hugo project.
    * `/shared`: Holds files and scripts shared between the `documentation` and `website` components, including AsciiDoc attributes and custom Asciidoctor macros.
    * `/tools`: Contains various utility scripts for tasks like translation and spellchecking.
    * `/.vale`: Contains configuration and styles for the Vale prose linter.

* **Language Organization:**
    * Under both `/documentation/content` and `/website/content`, source and translated material are organized into subdirectories named after [ISO 639-1] language codes (and variants where necessary).
    * Examples:
      ```
      bn-bd
      da
      de
      el
      en
      es
      fr
      hu
      id
      it
      ja
      ko
      mn
      nl
      pl
      pt-br
      ru
      tr
      zh-cn
      zh-tw
      ```
    * `en` (English) is the primary language and acts as the **source of truth**. All translations should be based on the latest English source files.

* **Translation Conventions for Chinese:**
    * `zh-cn`: Simplified Chinese translation, following **Mainland China conventions**:
        - Character Set: Simplified Chinese (简体中文).
        - Terminology: Use terminology consistent with PRC IT industry standards (e.g., 信息 instead of 資訊).
        - Punctuation: Use full-width Chinese punctuation where appropriate (e.g., `，` instead of `,`, `。` instead of `.`).
        - Quotation Marks: Always use `「」` for outer quotes (instead of English-style “ ”). Use `『』` for nested quotes when necessary.
        - Localization: Align with PRC technical norms and expression style, but maintain technical accuracy from the English original.
        - Examples: “FreeBSD Handbook” → 「FreeBSD 使用手册」.

    * `zh-tw`: Traditional Chinese translation, following **Taiwan conventions**:
        - Character Set: Traditional Chinese (繁體中文).
        - Terminology: Use Taiwan-prevalent IT terminology (e.g., 資訊 instead of 信息).
        - Punctuation: Follow Taiwan’s usage conventions for full-width punctuation.
        - Localization: Adapt phrasing to be natural for Taiwan readers while keeping fidelity to the English source.
        - Examples: “FreeBSD Handbook” → “FreeBSD 使用手冊”.

    * General guidance for both `zh-cn` and `zh-tw`:
        - Never machine-translate blindly; all technical terms should be verified against established FreeBSD usage and local community conventions.
        - Do not translate code, command names, configuration keys, or URLs.
        - Maintain AsciiDoc/Markdown syntax exactly as in the source (`=`, `==`, `*`, links, etc.).
        - Ensure that updates to `en` are tracked so `zh-cn` and `zh-tw` remain synchronized.

## 4. Coding Conventions & Style Guide

* **Formatting:**
    * Enforced by `.editorconfig` files.
    * **Indentation:** 2 spaces for `.adoc`, `.html`, and `.css` files.
    * **Line Endings:** Unix-style (`lf`).
    * **Charset:** UTF-8.
    * **Prose Style:** The Vale linter is used (`.vale.ini`) with the custom `FreeBSD` style to enforce writing conventions in AsciiDoc files.
* **Naming Conventions:**
    * `files`: Generally kebab-case or snake_case.
* **API Design:** Not applicable.
* **Error Handling:** Not applicable in the traditional sense. Build errors will be reported by the `make` and Hugo/Asciidoctor toolchain.

## 5. Key Files & Entrypoints

* **Main Entrypoint(s):** The top-level `Makefile` is the primary entrypoint for building the entire project via the `make all` command.
* **Configuration:**
    * `documentation/config/_default/hugo.toml`: Main Hugo configuration for the documentation.
    * `.vale.ini`: Configuration for the Vale prose linter.
    * `.cirrus.yml`: CI configuration for continuous builds.
* **CI/CD Pipeline:**
    * `.cirrus.yml`: Defines the primary CI process, which involves running `make` within a FreeBSD jail.
    * `.github/workflows/label-pull-requests.yml`: A GitHub Action to automatically label pull requests.

## 6. Development & Testing Workflow

* **Local Development Environment:**
    * The `README` file refers to the external "FreeBSD Documentation Project Primer" for detailed setup instructions.
    * The build is driven by `make`. A user would typically run `make all` from the root or `make` within the `documentation` or `website` subdirectories.
    * The `run` target in the sub-directory `Makefile`s suggests a local development server can be started (e.g., `cd documentation && make run`).
* **Testing:**
    * Prose is checked against a style guide using the Vale linter.
    * The CI process running `make` serves as the primary integration test to ensure the documentation builds successfully.
* **CI/CD Process:** When code is pushed, Cirrus CI automatically attempts to build the project using the `make` command in a dedicated FreeBSD environment.

## 7. Specific Instructions for AI Collaboration

* **Contribution Guidelines:** While no `CONTRIBUTING.md` file is present, the `.hooks/prepare-commit-msg` script contains strict guidelines for formatting commit messages.
* **Infrastructure (IaC):** Not applicable.
* **Security:** Be mindful of security. Do not hardcode secrets or keys.
* **Dependencies:** There is no package manager. New dependencies should not be added without understanding the project's established method for managing tools, which likely involves system-level installation within the FreeBSD environment.
* **Commit Messages:** Follow the template enforced by the `.hooks/prepare-commit-msg` script.
    * **Format:** `<Component>: Subject goes here, max 50 cols`
    * **Body:** 72-column line width.
    * **Metadata:** Include fields like `PR:`, `Reviewed by:`, `Approved by:`, etc., at the end of the message.
