
    (function() {
      var preconnectOrigins = ["https://cdn.shopify.com"];
      var scripts = ["/cdn/shopifycloud/checkout-web/assets/c1/polyfills.Db3KX98s.js","/cdn/shopifycloud/checkout-web/assets/c1/app.D6WOT6Dc.js","/cdn/shopifycloud/checkout-web/assets/c1/esnext-vendor.CDYRVrA6.js","/cdn/shopifycloud/checkout-web/assets/c1/context-browser.BPouX_2P.js","/cdn/shopifycloud/checkout-web/assets/c1/checkout-policy.B4FqN324.js","/cdn/shopifycloud/checkout-web/assets/c1/shop-pay-flags.Q3tFMxCb.js","/cdn/shopifycloud/checkout-web/assets/c1/page-rendered-hooks.BevIGKuy.js","/cdn/shopifycloud/checkout-web/assets/c1/shared-shop-theme.CIUFryrj.js","/cdn/shopifycloud/checkout-web/assets/c1/receipt-mapper-load-recovery.Dc_sUNWq.js","/cdn/shopifycloud/checkout-web/assets/c1/receipt-eager-mappers.BKb778d-.js","/cdn/shopifycloud/checkout-web/assets/c1/shop-pay-ProgressIntercepts.CjZDbzHt.js","/cdn/shopifycloud/checkout-web/assets/c1/error-logger-report-graphql-error.MH6xMxCL.js","/cdn/shopifycloud/checkout-web/assets/c1/graphql-PaymentSessionMutation.Pa8JOCBd.js","/cdn/shopifycloud/checkout-web/assets/c1/shop-pay-normalizeBuyerDetails.mBRwFbL3.js","/cdn/shopifycloud/checkout-web/assets/c1/mobile-checkout-sdk-MobileCheckoutSdkClient.DZrXKV2s.js","/cdn/shopifycloud/checkout-web/assets/c1/utilities-shopCashMoney.BIYrnghg.js","/cdn/shopifycloud/checkout-web/assets/c1/utilities-browser.CiABu55u.js","/cdn/shopifycloud/checkout-web/assets/c1/hydrate.C10flmQe.js","/cdn/shopifycloud/checkout-web/assets/c1/shop-pay-installments-monorail.z3hgPhiP.js","/cdn/shopifycloud/checkout-web/assets/c1/locale-en.DpdFvwA_.js","/cdn/shopifycloud/checkout-web/assets/c1/OnePage.CBnsVa8v.js","/cdn/shopifycloud/checkout-web/assets/c1/components-DeliveryTransition.DMdzKM_g.js","/cdn/shopifycloud/checkout-web/assets/c1/useShopPayButtonClassName.3xvMejdq.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useShowShopPayOptin.BLm21_tQ.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-usePickupPoints.BPYHVWp-.js","/cdn/shopifycloud/checkout-web/assets/c1/ChangeCompanyLocationLink.BQq4VfNy.js","/cdn/shopifycloud/checkout-web/assets/c1/BillingAddressForm.DHQOW5QC.js","/cdn/shopifycloud/checkout-web/assets/c1/PhoneField.B2759Krg.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useSuppressShopPayModalOnLoad.MMlKeB3T.js","/cdn/shopifycloud/checkout-web/assets/c1/utilities-compact.CETvGiEj.js","/cdn/shopifycloud/checkout-web/assets/c1/Popover.2oj9pKjR.js","/cdn/shopifycloud/checkout-web/assets/c1/Choice.CLvSKr0G.js","/cdn/shopifycloud/checkout-web/assets/c1/Checkbox.CxiJiLM8.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useUnauthenticatedErrorModal.AICD3xRN.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useCanChangeCompanyLocation.RTp9XI6x.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useForceShopPayUrl.BkJOVqDI.js","/cdn/shopifycloud/checkout-web/assets/c1/shipping-methods-grouping.B8ULaxYp.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useEcpSpiDebugLog.BuK7XDEF.js","/cdn/shopifycloud/checkout-web/assets/c1/ShopPayLogo.CAKlBWmi.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useWalletsTimeout.B6ueygTA.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-usePostPurchase.bLN7dhNr.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useWalletsMonorailTrack.CDXK6QE9.js","/cdn/shopifycloud/checkout-web/assets/c1/IncentiveBadge.CvZt5Sap.js","/cdn/shopifycloud/checkout-web/assets/c1/Section-SectionStyleOverride.DSjSRqu5.js","/cdn/shopifycloud/checkout-web/assets/c1/AutocompleteField-hooks.CIdDRSDc.js","/cdn/shopifycloud/checkout-web/assets/c1/PendingShipping.lgz6Iyxc.js","/cdn/shopifycloud/checkout-web/assets/c1/useAddressMutationsWithNegotiation.B41vPq5q.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentIcon.CfEXNnwb.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentLine.OXpqZOek.js","/cdn/shopifycloud/checkout-web/assets/c1/Theme-ThemeOverride.D4nuiwcP.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useUpdateCheckoutAddress.B4_-oEZw.js","/cdn/shopifycloud/checkout-web/assets/c1/payment-usePaymentExemptionReason.Bq5enxFk.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useShopPayProgressIntercepts.D1wbhc2M.js","/cdn/shopifycloud/checkout-web/assets/c1/Section.BUk-1LTY.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentErrorBanner.BrWL5ZWP.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useGeneralPaymentErrorMessage.DQVUsk8Y.js","/cdn/shopifycloud/checkout-web/assets/c1/StickyPayButton-StickyPayButton.module.COhA-1-6.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-payment-button.Q-X8U1Si.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-usePreselectSpi.BqOmLKAZ.js","/cdn/shopifycloud/checkout-web/assets/c1/Switch.CnKDcs5s.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useAvailableShopPromotionDiscounts.BcCC-sm_.js","/cdn/shopifycloud/checkout-web/assets/c1/Middot.DfQ7MVwO.js","/cdn/shopifycloud/checkout-web/assets/c1/EstimatedDeliveryContent.DWzYsn9r.js","/cdn/shopifycloud/checkout-web/assets/c1/ShippingMethodRateLabel.DU6VIb9v.js","/cdn/shopifycloud/checkout-web/assets/c1/shipping-methods-consolidated-included.fS9S3vCZ.js","/cdn/shopifycloud/checkout-web/assets/c1/ShippingLines.BEtfWkmv.js","/cdn/shopifycloud/checkout-web/assets/c1/ShipmentBreakdown.gtqyxztJ.js","/cdn/shopifycloud/checkout-web/assets/c1/MerchandiseModal.dVz5-Z1c.js","/cdn/shopifycloud/checkout-web/assets/c1/ShippingMethodSelector.C-LhSi3I.js","/cdn/shopifycloud/checkout-web/assets/c1/TextArea.BGQMwuMU.js","/cdn/shopifycloud/checkout-web/assets/c1/SubscriptionPriceBreakdown.C4lit580.js","/cdn/shopifycloud/checkout-web/assets/c1/StockProblems-StockProblemsLineItemList.B56_lqpd.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useShopPayNewSignupLoginExperiment.BvAs9pt9.js","/cdn/shopifycloud/checkout-web/assets/c1/page-BelowTheFoldContent.Dhg8tZs1.js","/cdn/shopifycloud/checkout-web/assets/c1/Captcha.DVMgVFBl.js","/cdn/shopifycloud/checkout-web/assets/c1/ShopPayCaptcha.CElUJO60.js","/cdn/shopifycloud/checkout-web/assets/c1/RememberMeSection.D6imMtJM.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentMethods.lq8p_ft7.js","/cdn/shopifycloud/checkout-web/assets/c1/MobileOrderSummary.7xLCrIuP.js","/cdn/shopifycloud/checkout-web/assets/c1/useShopPaySessionTokenStorage.BUh9OVQI.js","/cdn/shopifycloud/checkout-web/assets/c1/PayButtonSection.DPPaLEED.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentButtons.z2btLBcN.js","/cdn/shopifycloud/checkout-web/assets/c1/money-toShopPayMoneyInput.BjL6oioN.js","/cdn/shopifycloud/checkout-web/assets/c1/utils-useViolationsHandler.D2-Vz_7O.js","/cdn/shopifycloud/checkout-web/assets/c1/PaymentOptionSelector.DJIo06NU.js","/cdn/shopifycloud/checkout-web/assets/c1/BillingAddressSelector.DIxbvJOD.js","/cdn/shopifycloud/checkout-web/assets/c1/hooks-useStableHostMethodsReferences.BQUXrkn8.js"];
      var styles = ["/cdn/shopifycloud/checkout-web/assets/c1/assets/app.CM6PQxCl.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/checkout-policy.CdXX_CXd.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/previous.epZvFeZC.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/OnePage.Wbasq9un.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/DeliveryTransition.CxmS455s.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/useAddressMutationsWithNegotiation.DPEapfiO.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Section.CU18S7Ap.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PaymentLine.D3bcP-mr.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/StickyPayButton.3WRao8Y9.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PaymentIcon.gzvCNwz_.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/useShopPayProgressIntercepts.CIy8uDiZ.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Choice.DNWz77j7.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/IncentiveBadge.Dlnp55te.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/BillingAddressForm.BdwN7V1K.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Switch.Dq_6Ius6.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/useShopPayButtonClassName.CpHF4L7Q.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PhoneField.uZEuHncj.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Middot.D7Ujmshx.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/ShippingLines.LcqrKXE1.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/MerchandiseModal.D6OuIVjc.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/EstimatedDeliveryContent.B_THySFF.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/BelowTheFoldContent.CxfTiM6_.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Captcha.CJQgLR0i.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/RememberMeSection.JBO5WNhc.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/MobileOrderSummary.2B5x30PG.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PayButtonSection.CZuxzxFG.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PaymentOptionSelector.s-Kd_X2E.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PaymentMethods.CyNKgOhf.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/PaymentButtons.CRAl4z62.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Checkbox.CfwUdlpL.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/useShopPaySessionTokenStorage.DfWUBaTh.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/Popover.Bi1nHaU-.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/ShippingMethodSelector.B0hio2RO.css","/cdn/shopifycloud/checkout-web/assets/c1/assets/SubscriptionPriceBreakdown.vTcdVGq4.css"];
      var fontPreconnectUrls = [];
      var fontPrefetchUrls = [];
      var imgPrefetchUrls = ["https://cdn.shopify.com/s/files/1/0021/3984/8751/files/simple_invoice_account_logo_34193_c9e50a98be41f3492385bf518629ce07_x320.png?v=1737072669"];

      function preconnect(url, callback) {
        var link = document.createElement('link');
        link.rel = 'dns-prefetch preconnect';
        link.href = url;
        link.crossOrigin = '';
        link.onload = link.onerror = callback;
        document.head.appendChild(link);
      }

      function preconnectAssets() {
        var resources = preconnectOrigins.concat(fontPreconnectUrls);
        var index = 0;
        (function next() {
          var res = resources[index++];
          if (res) preconnect(res, next);
        })();
      }

      function prefetch(url, as, callback) {
        var link = document.createElement('link');
        if (link.relList.supports('prefetch')) {
          link.rel = 'prefetch';
          link.fetchPriority = 'low';
          link.as = as;
          if (as === 'font') link.type = 'font/woff2';
          link.href = url;
          link.crossOrigin = '';
          link.onload = link.onerror = callback;
          document.head.appendChild(link);
        } else {
          var xhr = new XMLHttpRequest();
          xhr.open('GET', url, true);
          xhr.onloadend = callback;
          xhr.send();
        }
      }

      function prefetchAssets() {
        var resources = [].concat(
          scripts.map(function(url) { return [url, 'script']; }),
          styles.map(function(url) { return [url, 'style']; }),
          fontPrefetchUrls.map(function(url) { return [url, 'font']; }),
          imgPrefetchUrls.map(function(url) { return [url, 'image']; })
        );
        var index = 0;
        function run() {
          var res = resources[index++];
          if (res) prefetch(res[0], res[1], next);
        }
        var next = (self.requestIdleCallback || setTimeout).bind(self, run);
        next();
      }

      function onLoaded() {
        try {
          if (parseFloat(navigator.connection.effectiveType) > 2 && !navigator.connection.saveData) {
            preconnectAssets();
            prefetchAssets();
          }
        } catch (e) {}
      }

      if (document.readyState === 'complete') {
        onLoaded();
      } else {
        addEventListener('load', onLoaded);
      }
    })();
  