//
// Created by david on 3/28/21.
//

#ifndef AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP
#define AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP

namespace esp_modem::DCE {


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

//    template <typename T, typename ...Args>
//    static T* make(Args&&... args)
//    {
//        return new T(std::forward<Args>(args)...);
//    }
//    template <typename T, typename ...Args>
//    static std::shared_ptr<T> make(Args&&... args)
//    {
//        return std::make_shared<T>(std::forward<Args>(args)...);
//    }
//    template <typename T, typename ...Args>
//    static std::unique_ptr<T> make(Args&&... args)
//    {
//        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
//    }


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
            throw_if_false(netif == nullptr, "Netif created but never used");
            throw_if_false(dte == nullptr, "dte was captured but never used");
        }

        DCE_T<T>* create(std::string &apn)
        {
            return create_dce(dte, netif, apn);
        }

        template<typename U>
        bool create_module(U &t, std::string &apn)
        {
            auto pdp = FactoryHelper::create_pdp_context(apn);
//        t = new T(std::move(dte), std::move(pdp));
            return FactoryHelper::make<T>(t, std::move(dte), std::move(pdp));
            return true;
        }

//    bool create_module(std::shared_ptr<T> &t, std::string &apn)
//    {
//        auto pdp = FactoryHelper::create_pdp_context(apn);
//        t = std::make_shared<T>(std::move(dte), std::move(pdp));
//        return true;
//    }

        template<typename U, typename V>
        U create_device(std::string &apn)
        {
            auto pdp = FactoryHelper::create_pdp_context(apn);
            return FactoryHelper::make<V>(std::move(dte), std::move(pdp));
        }

        T* create_dev(std::string &apn)
        {
            auto pdp = FactoryHelper::create_pdp_context(apn);
            return new T(std::move(dte), std::move(pdp));
        }

        std::shared_ptr<T> create_shared_dev(std::string &apn)
        {
            auto pdp = FactoryHelper::create_pdp_context(apn);
            return std::make_shared<T>(std::move(dte), std::move(pdp));
        }

        static DCE_T<T>* create_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<T>& dev, esp_netif_t *netif)
        {
            return new DCE_T<T>(dte, dev, netif);
        }

        static std::unique_ptr<DCE_T<T>> create_unique_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<T>& dev, esp_netif_t *netif)
        {
            return std::unique_ptr<DCE_T<T>>(new DCE_T<T>(dte, dev, netif));
        }

        static std::shared_ptr<T> create_module(const std::shared_ptr<DTE>& dte, std::string &apn)
        {
            auto pdp = FactoryHelper::create_pdp_context(apn);
            return std::make_shared<T>(dte, std::move(pdp));
        }

        static std::unique_ptr<DCE_T<T>> create_unique_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
        {
            auto module = create_module(dte, apn);
            return create_unique_dce_from_module(dte, std::move(module), netif);
        }

        static DCE_T<T>* create_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
        {
            auto module = create_module(dte, apn);
            return create_dce_from_module(dte, std::move(module), netif);
        }

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

        template <typename T, typename ...Args>
        T* create(Builder<T> b, std::string apn)
        {
            return b.create_dev(apn);
        }

        template <typename T, typename ...Args>
        std::shared_ptr<T> create(Builder<T> b, std::string apn)
        {
            return b.create_shared_dev(apn);
        }

        template <typename T, typename U, typename ...Args>
//    std::shared_ptr<T> build_module_T(Args&&... args)
//    bool build_module_T(std::shared_ptr<T> &t, Args&&... args)
        bool build_module_T(U &t, Args&&... args)
        {
            Builder<T> b(std::forward<Args>(args)...);
            std::string apn = "internet";
            return b.create_module(t, apn);
//        return b.create_shared_dev(apn);
        }
//    template <typename T, typename ...Args>
//    T* build_module_T(Args&&... args)
//    {
//        Builder<T> b(std::forward<Args>(args)...);
//        std::string apn = "internet";
//        return b.create_dev(apn);
//    }

        template <typename T, typename U, typename ...Args>
        T build_module_T(Args&&... args)
        {
//        Builder<U> b(std::forward<Args>(args)...);
//        std::string apn = "internet";
//        return b.template create_device<T>(apn);


            T module;
            if (build_module_T<U>(module, std::forward<Args>(args)...))
                return module;
            return nullptr;
        }

        template <typename T, typename ...Args>
        T build_module_xxT(Args&&... args)
        {
            T generic_module = nullptr;
            switch (m) {
                case Modem::MinModule:
                    break;
                case Modem::SIM7600: {
                    SIM7600 *module;
                    if (build_module_T<SIM7600>(module, std::forward<Args>(args)...))
                        generic_module = module;
                    break;
                }
                case Modem::SIM800: {
                    SIM800 *module;
                    if (build_module_T<SIM800>(module, std::forward<Args>(args)...))
                        generic_module = module;
                    break;
                }
                case Modem::BG96: {
                    BG96 *module;
                    if (build_module_T<BG96>(module, std::forward<Args>(args)...))
                        generic_module = module;
                    break;
                }
            }
            return generic_module;
        }

        template <typename T, typename ...Args>
        std::shared_ptr<T> build_shared_module_specific(Args&&... args)
        {
            return build_module_T<std::shared_ptr<T>, T>(std::forward<Args>(args)...);
        }

        template <typename ...Args>
        std::shared_ptr<GenericModule> build_shared_module(Args&&... args)
        {
//        Builder<GenericModule> b(std::forward<Args>(args)...);
//        std::string apn = "internet";
//        return b.template create_device<GenericModule, std::shared_ptr<GenericModule>>(apn);

            return build_shared_module_specific<SIM7600>(std::forward<Args>(args)...);
        }


    private:
        Modem m;
    };

} // esp_modem

#endif //AP_TO_PPPOS_ESP_MODEM_DCE_FACTORY_HPP
