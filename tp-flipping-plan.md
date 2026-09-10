# GW2 Trading Post Flipping — Araştırma & Addon Planı

## Bölüm 1: Araştırma Bulguları (Eylül 2026)

### TP Vergi Yapısı
- Listeleme ücreti: %5 (peşin, iade yok)
- Satış komisyonu: %10
- **Toplam vergi: %15**
- **Kâra geçiş eşiği: min. %17.65 spread**
- Formül: `Kâr = (Satış Fiyatı × 0.85) − Alış Fiyatı`

### Sıralama Metriği
Tablolar **günlük satış hacmi × birim kâr** üzerinden sıralanmıştır.
ROI tek başına yanıltıcıdır — %106 ROI'li 7c'lik bir item, günde sadece 445 adet
satılıyorsa toplam 31s/gün potansiyel taşır. Asıl önemli olan sirkülasyon hızıdır.

---

### Düşük Birim Fiyat (birim <1s)
Az altınla başlanabilir. Birim fiyat düşük, hacim yüksek. Altın/gün'e göre sıralı.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Pile of Coarse Sand | 77c | 1s13c | 19c | %24.7 | 10,926 | ~20g 76s |
| 2 | Green Wood Plank | 20c | 42c | 16c | %78.5 | 12,177 | ~19g 48s |
| 3 | Powdered Rose Quartz | 47c | 65c | 8c | %17.6 | 18,979 | ~15g 18s |
| 4 | Congealed Putrescence | 13c | 23c | 7c | %50.4 | 17,443 | ~12g 21s |
| 5 | Walnut | 42c | 62c | 11c | %25.5 | 11,055 | ~12g 16s |
| 6 | Seasoned Wood Log | 45c | 64c | 9c | %20.9 | 13,188 | ~11g 87s |

### Düşük Sermaye — İnce Marj (Dikkatli ol)
Çok yüksek günlük işlem, ancak marj %15 eşiğine yakın. Küçük fiyat değişiminde zarar riski.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Hard Wood Log | 75c | 95c | 6c | %7.7 | 31,490 | ~18g 89s |
| 2 | Eye of Kormir | 26c | 37c | 5c | %21 | 23,263 | ~11g 63s |
| 3 | Glass of Buttered Spirits | 6c | 13c | 5c | %84.2 | 7,008 | ~3g 50s |

### Yüksek Birim Fiyat (birim 1s+)
Birim başına daha fazla kâr, undercut'a karşı daha dayanıklı. Altın/gün'e göre sıralı.

| # | Item | Alış | Satış | Kâr | ROI | Gün/Satış | ~Altın/Gün |
|---|------|------|-------|-----|-----|-----------|------------|
| 1 | Super Loot Bag | 94s86c | 1g22s95c | 9s65c | %10.2 | 3,686 | ~355g pazar |
| 2 | Bag of Radiant Energy | 52s62c | 87s | 21s33c | %40.5 | 335 | ~71g 46s |
| 3 | Brilliant Opal Jewel | 1s30c | 3s98c | 2s8c | %160 | 715 | ~14g 89s |
| 4 | Corsair Tuning Crystal | 78c | 1s45c | 45c | %57.7 | 1,822 | ~8g 20s |
| 5 | Candy Corn Almond Brittle | 1s5c | 1s92c | 58c | %55.4 | 467 | ~2g 71s ⚠️ Halloween riski |

### KAÇINILACAK İtemler
- **Mystic Coin** — -%11.8 ROI (GW2 API: buy 195s90c / sell 203s19c → net -23s19c)
- **Glob of Ectoplasm** — Negatif marj
- **Strawberry Ghost** — Halloween (Ekim) öncesi fiyat çökme riski
- **Candy Corn Almond Brittle** — Aynı Halloween riski (mevsimsel item)

### Alternatif Stratejiler
1. **Time-Gated Crafting** (~1-3g/gün): Deldrimor Steel, Bolt of Damask, Elonian Leather, Spiritwood
2. **Rare Salvage → Ecto**: Level 68+ rare al → salvage → ~0.9 ecto/rare
3. **Mystic Forge T5→T6**: 50 T5 + 1 T6 + 5 Dust + 5 Philosopher's Stone = ort. [doğrulanacak] T6

### Risk Kuralları
- Tek iteme likit altının max %10-20'si
- Toplam altının %20-40'ını nakit tut
- 1c undercut 7c marjın %14'ünü siler — düşük copper itemlerde dikkat
- "Gün/Satış" toplam pazardır, bir oyuncunun payı ~%5-15
- İşlem öncesi GW2BLTC'den güncel fiyat MUTLAKA kontrol et

---

## Bölüm 2: Nexus Addon — GW2 TP Karar Destek Aracı

### Platform: Raidcore Nexus
Kullanıcı zaten Nexus kurulu ve aktif kullanıyor. Addon bu framework üzerine inşa edilecek.

- **Framework**: Nexus (Raidcore) — DX11 proxy DLL, oyun içi yönetim (CTRL+O)
- **Dil**: C++ (resmi SDK)
- **UI**: Dear ImGui v1.92.7 (Nexus tarafından sağlanır)
- **ToS**: Nexus ArenaNet Third Party Policy'ye uygun, kaynak kodu açık/denetlenebilir
- **HTTP**: WinHTTP (Windows native, Schannel TLS, sıfır ek bağımlılık — FarmingTracker ile aynı)
- **Referans addon**: FarmingTracker (xliviax) — GW2 API + API key kullanımı kanıtlanmış
- **Kullanım**: Kişisel — sadece sen kullanacaksın, dağıtım planı yok
- **Başlangıç sermayesi**: ~100g (copper-tier + bazı silver-tier itemler)
- **Crafting**: Şu anda 400+ disiplin yok — Crafting modülü (Faz 4) düşük öncelik
- **C++ + VS 2022**: Kurulu ve hazır
- **Durum**: Plan tamamlanıyor, kodlamaya henüz başlanmadı

### Dil Kararı: Neden C++?
- Tek kanıtlanmış Nexus + API key referans (FarmingTracker) C++ ile yazılmış
- Resmi template C++ — en çok örnek ve topluluk desteği burada
- std::thread yeterli (async runtime gereksiz — 5dk'da 1 HTTP istek)
- Rust/nexus-rs alternatif ama daha az topluluk örneği var

### HTTPS Stack Kararı: WinHTTP (Kesinleşti)
FarmingTracker kaynak kodu incelendi → **WinHTTP** kullanıyor. Kanıtlanmış, sıfır ek bağımlılık.
- `winhttp.h` + `#pragma comment(lib, "winhttp.lib")`
- Schannel TLS (Windows native) — OpenSSL gereksiz
- Pattern: `WinHttpOpen → WinHttpConnect → WinHttpOpenRequest(SECURE) → Send → Receive → Read loop`
- Threading: `std::thread` + `atomic<bool>` stop flag + `condition_variable` + `join()` on Shutdown

### ÖNEMLİ: Sınırlamalar, Riskler ve Çözümleri
| Sınırlama / Risk | Gerçek | Çözüm |
|-------------------|--------|-------|
| API read-only, emir veremez | Evet | Addon KARAR VERİR, sen TP'de tıklarsın |
| Günlük satış hacmi API'de yok | Evet | `/listings` quantity diff ile kaba tahmin (watchlist itemleri); kesin hacim için GW2BLTC referans |
| Rate limit | 300 burst, 5/sn refill, max 200 ID/istek (wiki doğrulandı) | 30 item = 1 istek/poll. 5dk aralıkla = 1 istek/5dk. Limit sorun değil |
| ArenaNet ban riski | Nexus ToS uyumlu | Sadece resmi API + ImGui overlay, memory reading YOK |
| **OYUN DONMASI RİSKİ** | Nexus ImGui callback render thread'de çalışır | **HTTP ASLA render callback'te çağrılmaz**. Worker thread poll eder → snapshot'ı mutex altında yazar → render sadece okur. Bu #1 Nexus addon hatası. |
| **API downtime** | Her patch'te ve rastgele kapanır | Son geçerli fiyatları cache'le, zamandamgası göster ("5dk önce" / "47dk önce — VERİ ESKİ"), hata durumunda hang etme |
| **Undercut relist maliyeti** | Satış emrini iptal etmek %5 listeleme ücretini KAYBEDER | hold_net, relist_net, relist_maliyeti + kuyruk bekleme süresi gösterir. Karar kullanıcıda — relist_net < 0 ise KIRMIZI uyarı |
| **İşlem geçmişi 90 gün** | Wiki doğrulandı: history sadece son 90 gün | P&L modülü 1. günden itibaren lokal JSON/SQLite'a kaydetmeli, yoksa eski veriler kaybolur |
| **Mystic Forge reçeteleri** | `/v2/recipes`'te YOK (MF reçeteleri API dışı) | T5→T6 promotion, ecto→dust gibi MF reçetelerini hardcode et. Philosopher's Stone = spirit shard (TP'de satılmaz) → kullanıcıdan shard değeri girişi iste veya 0 kabul et |
| **Addon Unload crash** | Nexus hot-reload yapar; worker thread HTTP ortasındayken Unload gelirse game crash | Atomic stop flag + kısa HTTP timeout (5-10s) + `join()` Unload'da + static state reset (Load tekrar çağrılabilir) |
| **Sadece oyun açıkken çalışır** | Addon = DLL, GW2 kapanınca durur | Fiyat geçmişi ve hacim tahmini boşluklu olacak — kabul edilen kısıtlama. Gelecekte: companion background service |
| **Item isim→ID eşlemesi** | `/v2/items` name search desteklemez | Watchlist'e item eklerken: ya lokal name→ID index (tek seferlik tüm ID'ler indir) ya da ID ile ekleme. Varsayılan watchlist item ID'leri hardcode |
| **Vendor/karma malzemeleri** | Crafting reçetelerinde TP dışı malzemeler var (ör. Thermocatalytic Reagent) | Bilinen vendor fiyatlarını hardcode et veya "manuel fiyat gir" alanı ekle |
| **MF T5→T6 ortalama 6.05 doğrulanmamış** | Kaynaklar farklı değerler veriyor | Faz 4'te gw2lunchbox'tan güncel yield ortalamasını al, hardcode etmeden önce doğrula |
| **ImGui ABI uyumu** | Farklı ImGui versiyonu = struct layout crash | Nexus template'in sağladığı ImGui header'ları ile derle, kendi ImGui versiyonunu ekleme |

### Konsept
Oyun içi ImGui overlay — TP açıkken veya kapalıyken çalışır:
- **Flipping Yardımcısı**: Watchlist itemlerinde canlı kâr/zarar hesabı
- **Crafting Kâr Hesaplayıcı**: Malzeme al → craft et → sat döngüsünde kâr analizi
- **Kişisel P&L**: Kendi işlem geçmişinden gerçek kâr/zarar raporu
- **Undercut Uyarısı**: Açık satış emirlerinde birisi altına girerse bildirim
- **Spread Alert**: İzlenen itemlerde kârlı fırsat oluşunca bildirim

### GW2 API Endpoint'leri
| Endpoint | Auth | Kullanım |
|----------|------|----------|
| `/v2/commerce/prices?ids=...` | Yok | Anlık buy/sell fiyatları |
| `/v2/commerce/listings/{id}` | Yok | Emir defteri derinliği |
| `/v2/commerce/transactions/current/buys` | API Key | Açık alış emirlerim |
| `/v2/commerce/transactions/current/sells` | API Key | Açık satış emirlerim |
| `/v2/commerce/transactions/history/buys` | API Key | Geçmiş alışlarım |
| `/v2/commerce/transactions/history/sells` | API Key | Geçmiş satışlarım |
| `/v2/items?ids=...` | Yok | Item meta (isim, ikon, tip) |
| `/v2/recipes/search?output={id}` | Yok | Crafting reçeteleri |
| `/v2/recipes/{id}` | Yok | Reçete detayı (malzemeler) |
| API Key oluşturma | — | account.arena.net/applications → `account` + `tradingpost` scope |

### Nexus Addon Teknik Mimari

```
GW2-TP-Assistant.dll (Nexus Addon)
├── Core
│   ├── HttpClient (cpp-httplib veya WinHTTP)
│   ├── GW2ApiClient (tüm endpoint sarmalayıcıları)
│   ├── ProfitEngine (%15 vergi hesabı, spread analizi)
│   └── ConfigManager (API key, watchlist, ayarlar — JSON)
├── Modüller
│   ├── FlipTracker (watchlist + spread alert + polling)
│   ├── CraftingCalc (reçete çözümleme + malzeme maliyet + kâr)
│   ├── PnLTracker (kişisel işlem geçmişi + kâr/zarar)
│   ├── UndercutDetector (açık emirler vs güncel listings)
│   └── PriceHistory (kendi topladığı fiyat verisi, lokal SQLite/JSON)
├── UI (ImGui)
│   ├── MainWindow (tab'lı ana pencere)
│   ├── FlipPanel (watchlist tablosu, renk kodlu kâr/zarar)
│   ├── CraftingPanel (reçete seçici, malzeme listesi, kâr özeti)
│   ├── PnLPanel (işlem geçmişi tablosu, toplam kâr)
│   ├── AlertPanel (son uyarılar listesi)
│   └── SettingsPanel (API key girişi, polling aralığı, watchlist düzenleme)
└── Nexus Entegrasyonu
    ├── Quick Access Bar ikonu
    ├── Keybind (toggle overlay)
    └── Settings (Nexus ayarlar menüsünde)
```

### Özellik Detayları

#### 1. Flip Tracker (Ana Özellik)
- Watchlist'e eklenen itemleri düzenli aralıklarla poll et (varsayılan: 5dk)
- Her item için: buy price, sell price, spread %, birim kâr (vergi sonrası)
- Renk kodu: yeşil (ROI >%5), sarı (ROI %0-5), kırmızı (ROI <0 — zarar)
- Spread alert: Belirlenen eşiğin üstüne çıkınca bildirim (Nexus Alerts_Notify varsa o, yoksa ImGui popup)
- **Varsayılan watchlist**: Copper-tier itemleri (Coarse Sand, Green Wood Plank, Walnut vb.)
- Hacim tahmini: `/listings` quantity diff ile kaba tahmin (her poll'da quantity farkı = satış tahmini). Kesin hacim için GW2BLTC referans

#### 2. Crafting Kâr Hesaplayıcı
- `/v2/recipes/search` ile item'ın reçetesini bul
- Tüm malzemelerin canlı TP fiyatını çek
- Toplam maliyet vs satış fiyatı (vergi dahil) karşılaştır
- Özel: Time-gated malzemeler (Deldrimor Steel vb.) için günlük kâr hesabı
- Özel: Mystic Forge T5→T6 promotion kâr hesabı (ort. [doğrulanacak] çıkış)

#### 3. Kişisel P&L (Kâr/Zarar Raporu)
- `/transactions/history` üzerinden gerçekleşmiş işlemler
- Item bazında: kaç adet aldın, kaç sattın, ortalama alış/satış, net kâr
- Oturum bazında ve toplam kâr/zarar
- Vergi kesintisi ayrı gösterilir

#### 4. Undercut Dedektörü
- `/transactions/current/sells` — senin açık satış emirlerin
- `/listings/{id}` — güncel emir defterinin tepesi
- Senin fiyatının altına birisi girerse → alert + 3 bilgi göster:
  ```
  hold_net       = eski_fiyat × 0.85 − alış_fiyat     (beklersen kâr)
  relist_net     = yeni_fiyat × 0.85 − alış − eski × 0.05  (relist edersen kâr)
  relist_maliyeti = hold_net − relist_net               (relist'in bedeli)
  ```
  Ayrıca `/listings/{id}`'den: senin altındaki birim sayısı ÷ tahmini günlük hacim
  = bekleme süresi tahmini ("altında 340 birim, ~12K/gün → ~40dk bekle")
  - `relist_net < 0` → KIRMIZI "relist zarar, BEKLE"
  - `relist_net > 0` → üç sayıyı göster, karar kullanıcıda
  - Yüksek hacimli itemlerde (Coarse Sand vb.) beklemek genelde doğru
  - Düşük hacimli itemlerde (Bag of Radiant Energy, 335/gün) relist gerekebilir
  - **Uyarı**: Formül satış olasılığını içermez — kuyruktaki birim sayısı karar girdisi

#### 5. Salvage Kâr Hesaplayıcı (Ek)
- Level 68+ Rare ekipman fiyatı vs (0.9 × ecto fiyatı × 0.85)
- Kit maliyeti dahil
- Kârlıysa yeşil, değilse kırmızı

### Mimari Kural: Render Thread Güvenliği
```
[Worker Thread]                    [Render Thread (ImGui)]
   │                                      │
   ├─ 5dk'da bir HTTP poll ──┐            │
   │                         │            │
   ├─ JSON parse             │            │
   │                         │            │
   ├─ ProfitEngine hesapla   │            │
   │                         ▼            │
   └─ mutex.lock() ──► snapshot yaz       │
                                          │
                        mutex.lock() ◄──── render sadece OKUR
                        snapshot oku ────► ImGui tablo çiz
```
**KURAL**: HTTP, JSON parse, hesaplama → SADECE worker thread.
Render callback → SADECE mutex altında snapshot oku + ImGui çiz.
Bu kuralı bozmak = oyun donması.

---

## Bölüm 3: Yapılacaklar (TODO)

### Faz 0: Hazırlık
- [x] API doğrulandı: history 90 gün sınırlı (wiki), transactions verisi 5dk cache (wiki)
- [x] API key scope: `account` + `tradingpost` gerekli (wiki)
- [x] VS 2022 + C++17 ortamı kurulu
- [x] API rate limit doğrulandı (wiki Best Practices): 300 burst, 5/sn refill (300/dk), max 200 ID/istek
- [ ] API key oluştur: account.arena.net/applications → `account` + `tradingpost` scope
- [ ] `/v2/commerce/prices?ids=19976` test çağrısı — response yapısını doğrula
- [ ] Nexus addon template'i kur: github.com/RaidcoreGG/GW2Nexus-AddonTemplate
- [ ] Template README'den ImGui ABI versiyonunu doğrula (Nexus'un sağladığı header'lar ile derlenmeli — versiyon uyuşmazlığı = crash)
- [x] FarmingTracker kaynak kodu incelendi: WinHTTP + atomic stop + CV + join pattern
- [ ] nlohmann/json ekle
- [x] Watchlist varsayılan item ID'leri (API ile doğrulandı):
  | Item | ID |
  |------|----|
  | Pile of Coarse Sand | 71641 |
  | Green Wood Plank | 19710 |
  | Powdered Rose Quartz | 86269 |
  | Congealed Putrescence | 83757 |
  | Walnut | 12250 |
  | Seasoned Wood Log | 19727 |
  | Corsair Tuning Crystal | 86287 |
  | Brilliant Opal Jewel | 24542 |
  | Bag of Radiant Energy | 71730 |
  | Hard Wood Log | 19724 |
  | Eye of Kormir | 83103 |
  | Glass of Buttered Spirits | 77667 |
  | Super Loot Bag | 97982 |
  | Candy Corn Almond Brittle | 36077 |
- [ ] `git init` + `.gitignore` (config JSON'da API key var — repo'ya girmemeli)

### Faz 1: Temel Altyapı ✅
- [x] Console harness (test_harness.exe) — ProfitEngine, ConfigManager, GW2ApiClient test edildi
- [x] Nexus addon iskelet (entry.cpp) — DLL yapısı, ImGui render callback
- [x] AddonUnload güvenliği — atomic stop flag + CV mutex + thread join + static state reset
- [x] Worker thread — HTTP worker'da, render sadece snapshot okur
- [x] Mutex + snapshot pattern
- [x] HttpClient (WinHTTP, 10s timeout)
- [x] GW2ApiClient — prices, items, transactions (206 Partial Content destekli)
- [x] API hata yönetimi — son geçerli veri cache, zamandamgası gösterim
- [x] ConfigManager — API key, watchlist, JSON dosya, dizin otomatik oluşturma
- [x] ProfitEngine — iki ayrı ücret (min 1c), spread analizi, relist maliyet hesabı

### Faz 2: Flip Tracker + Alert ✅
- [x] Watchlist UI — tablo + item silme butonu (X) + item ekleme (ID ile)
- [x] Polling döngüsü — worker thread, ayarlanabilir aralık (slider)
- [x] Spread hesabı ve renk kodlu gösterim (yeşil >%5, sarı %0-5, kırmızı <0)
- [x] Spread alert — Nexus GUI_SendAlert + hysteresis dedup (%20 fire, %10 reset)
- [x] İlk poll sessiz — mevcut durumu kaydeder, alert basmaz
- [x] Alert pencere kapalıyken de çalışır (DrainAlerts render'dan önce)
- [x] Varsayılan watchlist (9 item, ID'ler hardcode)
- [x] Item ekleme — placeholder isim ("Item #ID"), worker otomatik çözer
- [x] ForcePoll — Yenile butonu + ekle/sil/sıfırla sonrası anında güncelleme
- [x] Render thread'de HTTP çağrısı YOK (add-by-ID placeholder pattern)

### Faz 3: P&L + Undercut ✅
- [x] Transaction history — sayfalı çekme (?page=N&page_size=200), int64_t ID
- [x] FIFO maliyet hesabı — tarihe göre sıralı (ISO-8601 lexicographic sort test edildi)
- [x] Eşleşmeyen satışlar ayrı gösteriliyor (unmatchedSellQty), kâr hesabından hariç
- [x] Lokal veri birikimi — map<int64_t, TR> ile merge, aynı ID tekrarlanmaz (test edildi)
- [x] HTTP render thread'den **tamamen** Worker'a taşındı (RequestPnL/RequestUndercut)
- [x] Item isim çözümleme — batch /v2/items, Worker thread'de
- [x] Undercut alış fiyatı — P&L'den FIFO avg cost kullanılıyor
- [x] Per-item "yoksay" toggle (checkbox) + ignored items toplam kârdan hariç
- [x] Tab'lı UI: Flip Tracker | Kar/Zarar | Undercut
- [x] Undercut alert — Nexus GUI_SendAlert ile bildirim
- [x] test_pnl.cpp: 6 unit test (basic FIFO, sort, reversed order, unmatched, ignored, merge)

### Faz 4: Crafting Hesaplayıcı (DÜŞÜK ÖNCELİK — şu anda 400+ disiplin yok)
Crafting disiplinleri yükseltildiğinde devreye girer.
Not: Time-gated crafting 450+ gerektirir. 0→500 leveling maliyetini gw2efficiency
crafting calculator ile hesapla, ~1-3g/gün kâr ile karşılaştır.
- [ ] Normal reçeteler: `/v2/recipes/search` + `/v2/recipes/{id}` ile çözümleme
- [ ] Malzeme maliyet hesabı (canlı fiyatlarla)
- [ ] Crafting kâr paneli (ImGui)
- [ ] Time-gated malzemeler (Deldrimor Steel vb.) — günlük kâr hesabı
- [ ] Mystic Forge reçeteleri — API'de YOK, hardcode edilmeli:
  - T5→T6 promotion: 50 T5 + 1 T6 + 5 Dust + 5 Phil. Stone = ort. [gw2lunchbox'tan doğrula] T6
  - Ecto→Dust: 1 ecto = ort. 1.84 dust
  - Phil. Stone = spirit shard (TP'de satılmaz) → kullanıcıdan shard değeri iste veya 0 kabul et
- [ ] Vendor/karma malzemeleri — bilinen vendor fiyatlarını hardcode et (ör. Thermocatalytic Reagent)
- [ ] Crafting disiplin kontrolü — `/v2/characters/:id/crafting` → `characters` scope (opsiyonel)

### Faz 5: Ek Özellikler
- [ ] Salvage kâr hesaplayıcı
- [ ] Fiyat geçmişi (lokal depolama + grafik)
- [ ] Emir defteri derinliği görselleştirme
- [ ] Türkçe UI (ana dil)
