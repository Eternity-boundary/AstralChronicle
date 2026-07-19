// Created by EternityBoundary on Jul 20,2026
#pragma once

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace AstralChronicle::core
{
    class ServiceProvider final
    {
    public:
        template <typename TService>
        void AddSingleton(std::shared_ptr<TService> service)
        {
            if (!service)
            {
                throw std::invalid_argument("A service singleton cannot be null.");
            }

            m_services.insert_or_assign(std::type_index{ typeid(TService) }, std::move(service));
        }

        template <typename TService>
        [[nodiscard]] std::shared_ptr<TService> GetRequiredService() const
        {
            auto const service = m_services.find(std::type_index{ typeid(TService) });
            if (service == m_services.end())
            {
                throw std::logic_error("The requested service has not been registered.");
            }

            return std::static_pointer_cast<TService>(service->second);
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_services;
    };
}
