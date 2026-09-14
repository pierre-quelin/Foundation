/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ISecretProvider.hpp
 * @brief Shared secret resolution contract (config / DB / IPC). Declared only — no activation yet.
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tools::design::db
{

/**
 * @brief Resolves a secret reference (env name, file path, vault key, …).
 *
 * Same conceptual contract as config secrets (PasswordEnv / SecretRef). Implementations
 * (EnvSecretProvider, …) come later; until then @c open() may ignore a null provider.
 */
class ISecretProvider
{
public:
    virtual ~ISecretProvider() = default;

    /**
     * @brief Resolve @p ref to a secret value.
     * @return empty optional if the reference cannot be resolved.
     */
    [[nodiscard]] virtual std::optional<std::string> resolve(std::string_view ref) const = 0;
};

} // namespace tools::design::db
