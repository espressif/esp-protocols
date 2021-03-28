//
// Created by david on 3/28/21.
//

#ifndef AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP
#define AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP

#include <esp_log.h>

namespace esp_modem::DCE {

using config = esp_modem_dce_config;

    class FactoryHelper {
    public:
        static std::unique_ptr<PdpContext> create_pdp_context(std::string &apn);

        template <typename T, typename ...Args>
        static bool make(T* &t, Args&&... args)
        {
            t = new T(std::forward<Args>(args)...);
            return true;
        }

        template <typename T, typename ...Args>
        static bool make(std::shared_ptr<T> &t, Args&&... args)
        {
            t = std::make_shared<T>(std::forward<Args>(args)...);
            return true;
        }

        template <typename T, typename ...Args>
        static bool make(std::unique_ptr<T> &t, Args&&... args)
        {
            t = std::unique_ptr<T>(std::forward<Args>(args)...);
            return true;
        }

    };

    template<typename T>
    class Builder {
    public:
        explicit Builder(std::shared_ptr<DTE> dte): dte(std::move(dte))
        {
            esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_PPP();
            netif = esp_netif_new(&netif_config);
            throw_if_false(netif != nullptr, "Cannot create default PPP netif");
        }

        Builder(std::shared_ptr<DTE> dte, esp_netif_t* esp_netif): dte(std::move(dte)), module(nullptr), netif(esp_netif)
        {
            throw_if_false(netif != nullptr, "Null netif");
        }

        Builder(std::shared_ptr<DTE> dte, esp_netif_t* esp_netif, std::shared_ptr<T> dev): dte(std::move(dte)), module(std::move(dev)), netif(esp_netif)
        {
            throw_if_false(netif != nullptr, "Null netif");
        }

        ~Builder()
        {
//            throw_if_false(netif == nullptr, "Netif created but never used");
//            throw_if_false(dte.use_count() == 0, "dte was captured but never used");
            throw_if_false(module == nullptr, "module was captured or created but never used");
        }

//        DCE_T<T>* create(std::string &apn)
//        {
//            return create_dce(dte, netif, apn);
//        }

//        template<typename Ptr>
//        bool create_module(Ptr &t, std::string &apn)
//        {
//            auto pdp = FactoryHelper::create_pdp_context(apn);
//            return FactoryHelper::make<T>(t, std::move(dte), std::move(pdp));
//        }

        template<typename Ptr>
        bool create_module(Ptr &t, esp_modem_dce_config *config)
        {
            return FactoryHelper::make<T>(t, dte, config);
        }

        template<typename Ptr>
        bool create(Ptr &t, esp_modem_dce_config *config)
        {
            if (dte == nullptr)
                return false;
            if (module == nullptr) {
                if (!create_module(module, config)) {
                    ESP_LOGE("builder", "Failed to build module");
                    return false;

                }
            }
            ESP_LOGI("builder", "create_module passed");
            return FactoryHelper::make<DCE_T<T>>(t, std::move(dte), std::move(module), netif);
        }


//    bool create_module(std::shared_ptr<T> &t, std::string &apn)
//    {
//        auto pdp = FactoryHelper::create_pdp_context(apn);
//        t = std::make_shared<T>(std::move(dte), std::move(pdp));
//        return true;
//    }

//        template<typename U, typename V>
//        U create_device(std::string &apn)
//        {
//            auto pdp = FactoryHelper::create_pdp_context(apn);
//            return FactoryHelper::make<V>(std::move(dte), std::move(pdp));
//        }
//
//        T* create_dev(std::string &apn)
//        {
//            auto pdp = FactoryHelper::create_pdp_context(apn);
//            return new T(std::move(dte), std::move(pdp));
//        }
//
//        std::shared_ptr<T> create_shared_dev(std::string &apn)
//        {
//            auto pdp = FactoryHelper::create_pdp_context(apn);
//            return std::make_shared<T>(std::move(dte), std::move(pdp));
//        }
//
//        static DCE_T<T>* create_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<T>& dev, esp_netif_t *netif)
//        {
//            return new DCE_T<T>(dte, dev, netif);
//        }
//
//        static std::unique_ptr<DCE_T<T>> create_unique_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<T>& dev, esp_netif_t *netif)
//        {
//            return std::unique_ptr<DCE_T<T>>(new DCE_T<T>(dte, dev, netif));
//        }

//        static std::shared_ptr<T> create_module(const std::shared_ptr<DTE>& dte, std::string &apn)
//        {
//            auto pdp = FactoryHelper::create_pdp_context(apn);
//            return std::make_shared<T>(dte, std::move(pdp));
//        }
//
//        static std::unique_ptr<DCE_T<T>> create_unique_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
//        {
//            auto module = create_module(dte, apn);
//            return create_unique_dce_from_module(dte, std::move(module), netif);
//        }
//
//        static DCE_T<T>* create_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
//        {
//            auto module = create_module(dte, apn);
//            return create_dce_from_module(dte, std::move(module), netif);
//        }

    private:
        std::shared_ptr<DTE> dte;
        std::shared_ptr<T> module;
        esp_netif_t *netif;
    };

    enum class Modem {
        SIM800,
        SIM7600,
        BG96,
        MinModule
    };
    class Factory {
    public:
        explicit Factory(Modem modem): m(modem) {}

        template <typename T, typename U, typename ...Args>
        static bool build_module_T(U &t, config *cfg, Args&&... args)
        {
            Builder<T> b(std::forward<Args>(args)...);
            return b.create_module(t, cfg);
        }

        template <typename T, typename U, typename ...Args>
        static T build_module_T(config *cfg, Args&&... args)
        {
            T module;
            if (build_module_T<U>(module, cfg, std::forward<Args>(args)...))
                return module;
            return nullptr;
        }

        template <typename T, typename U, typename ...Args>
        static bool build_T(U &t, config *cfg, Args&&... args)
        {
            Builder<T> b(std::forward<Args>(args)...);
            return b.create(t, cfg);
        }

        template <typename T, typename U, typename ...Args>
        static T build_T(config *cfg, Args&&... args)
        {
            T dce;
            if (build_T<U>(dce, cfg, std::forward<Args>(args)...))
                return dce;
            return nullptr;
        }

        template <typename T, typename ...Args>
        static std::unique_ptr<DCE_T<T>> build_unique(config *cfg, Args&&... args)
        {
            return build_T<std::unique_ptr<DCE_T<T>>, T>(cfg, std::forward<Args>(args)...);
        }

        template <typename T, typename ...Args>
        static DCE_T<T>* build(config *cfg, Args&&... args)
        {
            return build_T<DCE_T<T>*, T>(cfg, std::forward<Args>(args)...);
        }


        template <typename T, typename ...Args>
        static std::shared_ptr<T> build_shared_module(config *cfg, Args&&... args)
        {
            return build_module_T<std::shared_ptr<T>, T>(cfg, std::forward<Args>(args)...);
        }


        template <typename ...Args>
        std::shared_ptr<GenericModule> build_shared_module(config *cfg, Args&&... args)
        {
            switch (m) {
                case Modem::SIM800:
                    return build_shared_module<SIM800>(cfg, std::forward<Args>(args)...);
                case Modem::SIM7600:
                    return build_shared_module<SIM7600>(cfg, std::forward<Args>(args)...);
                case Modem::BG96:
                    return build_shared_module<BG96>(cfg, std::forward<Args>(args)...);
                case Modem::MinModule:
                    break;
            }
            return nullptr;
        }


    private:
        Modem m;
    };

} // esp_modem

#endif //AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP
