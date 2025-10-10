// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file chargedJetHadron.cxx
/// \brief Charged-particle jet - hadron correlation task
/// \author Yongzhen Hou <yongzhen.hou@cern.ch>

#include "PWGHF/DataModel/CandidateReconstructionTables.h"
#include "PWGHF/DataModel/CandidateSelectionTables.h"
#include "PWGJE/Core/JetDerivedDataUtilities.h"
#include "PWGJE/Core/JetFindingUtilities.h"
#include "PWGJE/Core/JetHFUtilities.h"
#include "PWGJE/DataModel/Jet.h"
#include "PWGJE/DataModel/JetReducedData.h"
#include "PWGJE/DataModel/JetSubtraction.h"

#include "Framework/ASoA.h"
#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include <CommonConstants/MathConstants.h>
#include <Framework/Configurable.h>
#include <Framework/HistogramSpec.h>
#include <Framework/InitContext.h>
#include <Framework/OutputObjHeader.h> //refer-hfFF
#include <Framework/runDataProcessing.h>

///////////ref://///////mixed/////////////////
#include "Common/CCDB/EventSelectionParams.h"
#include "Common/Core/RecoDecay.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/TrackSelectionTables.h"

#include <CommonConstants/PhysicsConstants.h>
#include <Framework/ASoAHelpers.h>
#include <Framework/AnalysisDataModel.h>
#include <Framework/AnalysisHelpers.h>
#include <Framework/BinningPolicy.h>
#include <Framework/GroupedCombinations.h>
#include <Framework/O2DatabasePDGPlugin.h>

#include <THn.h>

#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::soa;

struct ChargedJetHadron {

  using CorrChargedJets = soa::Join<aod::ChargedJets, aod::ChargedJetConstituents>;
  using CorrChargedMCDJets = soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents>;
  using CorrChargedMCPJets = soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents>;
  using McParticleCollision = soa::Join<aod::JetMcCollisions, aod::BkgChargedMcRhos>;

  // using BinningType = ColumnBinningPolicy<aod::jcollision::PosZ, aod::cent::centFT0M>;
  using BinningType = ColumnBinningPolicy<aod::jcollision::PosZ, aod::mult::MultNTracksGlobal>;
  using BinningTypeMC = ColumnBinningPolicy<aod::jcollision::PosZ, aod::mult::MultNTracksGlobal>;
  // using BinningType = ColumnBinningPolicy<aod::jcollision::PosZ, aod::mult::MultFT0M<aod::mult::MultFT0A, aod::mult::MultFT0C>>;
  // using BinningType = ColumnBinningPolicy<aod::jcollision::PosZ, aod::jcollision::centFT0M>;

  Configurable<float> selectedJetsRadius{"selectedJetsRadius", 0.4, "resolution parameter for histograms without radius"};
  Configurable<std::string> eventSelections{"eventSelections", "sel8", "choose event selection"};
  Configurable<float> vertexZCut{"vertexZCut", 10.0f, "Accepted z-vertex range"};
  Configurable<float> centralityMin{"centralityMin", -999.0, "minimum centrality"};
  Configurable<float> centralityMax{"centralityMax", 999.0, "maximum centrality"};
  Configurable<float> leadingjetptMin{"leadingjetptMin", 20.0, "minimum leadingjetpt"};
  Configurable<float> subleadingjetptMin{"subleadingjetptMin", 10.0, "minimum subleadingjetpt"};
  Configurable<float> trackEtaMin{"trackEtaMin", -0.9, "minimum eta acceptance for tracks"};
  Configurable<float> trackEtaMax{"trackEtaMax", 0.9, "maximum eta acceptance for tracks"};
  Configurable<float> trackPtMin{"trackPtMin", 0.15, "minimum pT acceptance for tracks"};
  Configurable<float> trackPtMax{"trackPtMax", 100.0, "maximum pT acceptance for tracks"};
  Configurable<std::string> trackSelections{"trackSelections", "globalTracks", "set track selections"};
  Configurable<float> pTHatMaxMCD{"pTHatMaxMCD", 999.0, "maximum fraction of hard scattering for jet acceptance in detector MC"};
  Configurable<float> pTHatMaxMCP{"pTHatMaxMCP", 999.0, "maximum fraction of hard scattering for jet acceptance in particle MC"};
  Configurable<float> pTHatExponent{"pTHatExponent", 6.0, "exponent of the event weight for the calculation of pTHat"};
  Configurable<float> pTHatAbsoluteMin{"pTHatAbsoluteMin", -99.0, "minimum value of pTHat"};
  Configurable<double> jetPtMax{"jetPtMax", 200., "set jet pT bin max"};
  Configurable<float> jetEtaMin{"jetEtaMin", -0.7, "minimum jet pseudorapidity"};
  Configurable<float> jetEtaMax{"jetEtaMax", 0.7, "maximum jet pseudorapidity"};
  Configurable<float> jetAreaFractionMin{"jetAreaFractionMin", -99.0, "used to make a cut on the jet areas"};
  Configurable<float> leadingConstituentPtMin{"leadingConstituentPtMin", -99.0, "minimum pT selection on jet constituent"};
  Configurable<float> leadingConstituentPtMax{"leadingConstituentPtMax", 9999.0, "maximum pT selection on jet constituent"};
  Configurable<int> trackOccupancyInTimeRangeMax{"trackOccupancyInTimeRangeMax", 999999, "maximum track occupancy of tracks in neighbouring collisions in a given time range; only applied to reconstructed collisions (data and mcd jets), not mc collisions (mcp jets)"};
  Configurable<int> trackOccupancyInTimeRangeMin{"trackOccupancyInTimeRangeMin", -999999, "minimum track occupancy of tracks in neighbouring collisions in a given time range; only applied to reconstructed collisions (data and mcd jets), not mc collisions (mcp jets)"};
  Configurable<int> acceptSplitCollisions{"acceptSplitCollisions", 0, "0: only look at mcCollisions that are not split; 1: accept split mcCollisions, 2: accept split mcCollisions but only look at the first reco collision associated with it"};
  Configurable<bool> skipMBGapEvents{"skipMBGapEvents", false, "flag to choose to reject min. bias gap events; jet-level rejection can also be applied at the jet finder level for jets only, here rejection is applied for collision and track process functions for the first time, and on jets in case it was set to false at the jet finder level"};
  Configurable<bool> checkLeadConstituentPtForMcpJets{"checkLeadConstituentPtForMcpJets", false, "flag to choose whether particle level jets should have their lead track pt above leadingConstituentPtMin to be accepted; off by default, as leadingConstituentPtMin cut is only applied on MCD jets for the Pb-Pb analysis using pp MC anchored to Pb-Pb for the response matrix"};
  // 20250918 mixed events
  //
  Configurable<int> numberEventsMixed{"numberEventsMixed", 5, "number of events mixed in ME process"};
  ConfigurableAxis binsZVtx{"binsZVtx", {VARIABLE_WIDTH, -10.0f, -2.5f, 2.5f, 10.0f}, "Mixing bins - z-vertex"};
  //  ConfigurableAxis binsMultiplicity{"binsMultiplicity", {VARIABLE_WIDTH, 0.0f, 2000.0f, 6000.0f, 100000.0f}, "Mixing bins - multiplicity"}; // zhang zhen
  ConfigurableAxis binsMultiplicity{"binsMultiplicity", {VARIABLE_WIDTH, 0.0f, 15.0f, 25.0f, 35.0f, 50.0f}, "Mixing bins - multiplicity"}; // Julius Kinner photon-jet correlation analys
  ConfigurableAxis binsCentrality{"binsCentrality", {VARIABLE_WIDTH, 0.0, 10., 50, 100.}, "Mixing bins - centrality"};

  Configurable<int> CentEstimator{"cfgCentEstimator", 0, "0:FT0C; 1:FT0A; 2:FT0M"};
  // Filter ..................
  Filter trackCuts = (aod::jtrack::pt >= trackPtMin && aod::jtrack::pt < trackPtMax && aod::jtrack::eta > trackEtaMin && aod::jtrack::eta < trackEtaMax);
  Filter eventCuts = (nabs(aod::jcollision::posZ) < vertexZCut && aod::jcollision::centFT0M >= centralityMin && aod::jcollision::centFT0M < centralityMax); // 20250814 and collision.centrality-> collision.centFT0M

  SliceCache cache;
  using FilterCollision = soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos>>::iterator;
  using FilterCollisions = soa::Filtered<soa::Join<aod::JetCollisions, aod::BkgChargedRhos, aod::MultsGlobal>>;
  using FilterJetTracks = soa::Filtered<aod::JetTracks>;
  BinningType corrBinning{{binsZVtx, binsMultiplicity}, true};
  BinningTypeMC corrBinningMC{{binsZVtx, binsMultiplicity}, true};
  //  using CorrCollisions = soa::Join<aod::JetCollisions, aod::CollisionsExtraCorr, aod::MultsGlobal>;
  //  BinningType corrBinning{{binsZVtx, binsCentrality}, true};

  //////////////////////////////////////////
  HistogramRegistry registry;

  std::vector<int> eventSelectionBits;
  int trackSelection = -1;

  void init(o2::framework::InitContext&)
  {
    eventSelectionBits = jetderiveddatautilities::initialiseEventSelectionBits(static_cast<std::string>(eventSelections));
    trackSelection = jetderiveddatautilities::initialiseTrackSelection(static_cast<std::string>(trackSelections));

    AxisSpec centralityAxis = {110, -5., 105., "Centrality"};
    AxisSpec trackPtAxis = {200, -0.5, 199.5, "#it{p}_{T} (GeV/#it{c})"};
    AxisSpec etaAxis = {100, -1.0, 1.0, "#eta"};
    AxisSpec phiAxis = {160, -1.0, 7.0, "#varphi"};
    AxisSpec jetPtAxis = {200, 0., 200., "#it{p}_{T} (GeV/#it{c})"};
    AxisSpec jetPtAxisRhoAreaSub = {280, -80., 200., "#it{p}_{T} (GeV/#it{c})"};
    AxisSpec jetmultetaAxis = {100, -0.5, 0.5, "#Delta#eta"};
    AxisSpec dphiAxis = {140, -1.7, 5.3, "#Delta#varphi"};
    AxisSpec dphijetAxis = {160, -1.7, 6.3, "#Delta#varphi"};
    AxisSpec detaAxis = {170, -1.7, 1.7, "#Delta#eta"};
    AxisSpec drAxis = {110, -1.1, 1.1, "#Delta#it{R}"};

    if (doprocessCollisions || doprocessCollisionsWeighted) {
      registry.add("h_collisions", "event status;event status; entries", {HistType::kTH1F, {{4, 0.0, 4.0}}});
      registry.add("h_fakecollisions", "event status;event status; entries", {HistType::kTH1F, {{4, 0.0, 4.0}}});
      registry.add("h2_centrality_occupancy", "centrality vs occupancy; centrality; occupancy", {HistType::kTH2F, {centralityAxis, {60, 0, 30000}}});
      registry.add("h_collisions_Zvertex", "position of collision; #it{Z} (cm)", {HistType::kTH1F, {{300, -15.0, 15.0}}});
      registry.add("h_collisions_multFT0M", " multiplicity using multFT0M; entries", {HistType::kTH1F, {{3000, 0, 10000}}});
      registry.add("h_collisions_mult", " multiplicity globle tracks; entries", {HistType::kTH1F, {{1000, 0, 1000}}});
      if (doprocessCollisionsWeighted) {
        registry.add("h_collisions_weighted", "event status;event status;entries", {HistType::kTH1F, {{4, 0.0, 4.0}}});
      }
    }

    if (doprocessQC || doprocessQCWeighted) {
      registry.add("h_track_pt", "track #it{p}_{T} ; #it{p}_{T,track} (GeV/#it{c})", {HistType::kTH1F, {trackPtAxis}});
      registry.add("h2_track_eta_track_phi", "track eta vs. track phi; #eta; #phi; counts", {HistType::kTH2F, {etaAxis, phiAxis}});
    }

    if (doprocessSpectraData || doprocessSpectraMCD || doprocessSpectraMCDWeighted) {
      registry.add("h_jet_pt", "jet pT; #it{p}_{T,jet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_jet_eta", "jet eta; #eta_{jet}; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_jet_phi", "jet phi; #phi_{jet}; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_jet_area", "jet Area_{jet}; Area_{jet}; counts", {HistType::kTH1F, {{150, 0., 1.5}}});
      registry.add("h_jet_ntracks", "jet N_{jet tracks}; N_{jet, tracks; counts}", {HistType::kTH1F, {{200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_track_pt", "jet #it{p}_{T,jet} vs. #it{p}_{T,track}; #it{p}_{T,jet} (GeV/#it{c});  #it{p}_{T,track} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxis, trackPtAxis}});
      if (doprocessSpectraMCDWeighted) {
        registry.add("h_jet_phat", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
        registry.add("h_jet_phat_weighted", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
      }
    }

    if (doprocessSpectraAreaSubData || doprocessSpectraAreaSubMCD) {
      registry.add("h_jet_pt_rhoareasubtracted", "jet pt; #it{p}_{T,jet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_jet_eta_rhoareasubtracted", "jet eta; #eta_{jet}; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_jet_phi_rhoareasubtracted", "jet phi; #phi_{jet}; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_jet_area_rhoareasubtracted", "jet Area_{jet}; Area_{jet}; counts", {HistType::kTH1F, {{150, 0., 1.5}}});
      registry.add("h_jet_ntracks_rhoareasubtracted", "jet N_{jet tracks}; N_{jet, tracks; counts}", {HistType::kTH1F, {{200, -0.5, 199.5}}});
    }

    registry.add("h_centrality", "centrality distributions; centrality; counts", {HistType::kTH1F, {centralityAxis}});
    registry.add("h_inclusivejet_corrpt", "inclusive jet pT;#it{p}_{T,jet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
    registry.add("h_dijet_pair_counts", "number of paris with good leading-subleading jets; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
    registry.add("h_dijet_pair_counts_cut", "number of pairs with leadingjet & subleadingjet cut pair; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
    registry.add("h_leadjet_pt", "leading jet pT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
    registry.add("h_leadjet_corrpt", "leading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
    registry.add("h_subleadjet_pt", "subleading jet pT;#it{p}_{T,subleadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
    registry.add("h_subleadjet_corrpt", "subleading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
    registry.add("h_dijet_dphi", "dijet #Delta#varphi before converted to 0-2pi; #Delta#varphi; counts", {HistType::kTH1F, {{126, -6.3, 6.3}}});
    registry.add("h_leadjet_eta", "leading jet eta;#eta; counts", {HistType::kTH1F, {etaAxis}});
    registry.add("h_leadjet_phi", "leading jet phi;#phi; counts", {HistType::kTH1F, {phiAxis}});
    registry.add("h_subleadjet_eta", "subleading jet eta;#eta; counts", {HistType::kTH1F, {etaAxis}});
    registry.add("h_subleadjet_phi", "subleading jet phi;#phi; counts", {HistType::kTH1F, {phiAxis}});
    registry.add("h2_dijet_detanoflip_dphi", "dijet #Delta#eta no flip vs #Delta#varphi; #Delta#eta_{noflip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
    registry.add("h2_dijet_deta_dphi", "dijet #Delta#eta flip vs #Delta#varphi; #Delta#eta_{flip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
    registry.add("h2_dijet_Asymmetry", "dijet Asymmetry; #it{p}_{T,subleadingjet} (GeV/#it{c}); #it{X}_{J}; counts", {HistType::kTH2F, {jetPtAxisRhoAreaSub, {40, 0, 1.0}}});

    registry.add("h_jeth_detatot", "jet-hadron tot #Delta#eta;#Delta#eta; counts", {HistType::kTH1F, {detaAxis}});
    registry.add("h_jeth_deta", "jet-hadron #Delta#eta;#Delta#eta; counts", {HistType::kTH1F, {detaAxis}});
    registry.add("h_jeth_dphi", "jet-hadron #Delta#varphi;#Delta#varphi; counts", {HistType::kTH1F, {dphiAxis}});
    registry.add("h2_jeth_detatot_dphi", "jeth deta vs dphi with physical cuts; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_deta_dphi", "jeth deta vs dphi; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_physicalcutsup_deta_dphi", "jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| > 1.0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_physicalcutsmd_deta_dphi", "jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| #in (0.5, 1.0); #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_physicalcutsdw_deta_dphi", "jeth deta vs dphi with physical cuts  |#Delta#eta_{jet1,2}| < 0.5; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_physicalcutsHup_deta_dphi", "jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| > 0.5,#Delta#eta_{jet2}>0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("h2_jeth_physicalcutsHdw_deta_dphi", "jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| < 0.5 #Delta#eta_{jet2}>0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    registry.add("thn_jethadron_correlations", "jet-h correlations; leadingjetpT; subleadingjetpT; trackpT; no flip jeth#Delta#eta; #Delta#eta_{jet1,2}; jeth#Delta#eta; jeth#Delta#varphi", HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, detaAxis, detaAxis, dphiAxis});

    if (doprocessJetHadronCorrelation || doprocessMixJetHadronCorrelation) {
      registry.add("thn_jeth_correlations", "jet-h correlations; jetpT; trackpT; jeth#Delta#eta; jeth#Delta#varphi; jeth#Delta#it{R}", HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, dphiAxis, drAxis});
      registry.add("thn_mixjeth_correlations", "ME: jet-h correlations; jetpT; trackpT; jeth#Delta#eta; jeth#Delta#varphi; jeth#Delta#it{R}", HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, dphiAxis, drAxis});
      registry.add("h_jeth_event_stats", "Same event statistics; Event pair type; counts", {HistType::kTH1F, {{10, 0.5, 10.5}}});
      registry.get<TH1>(HIST("h_same_event_stats"))->GetXaxis()->SetBinLabel(2, "Total jets");
      registry.get<TH1>(HIST("h_same_event_stats"))->GetXaxis()->SetBinLabel(3, "Total jets with cuts");
      registry.get<TH1>(HIST("h_same_event_stats"))->GetXaxis()->SetBinLabel(4, "Total j-h pairs");
      registry.get<TH1>(HIST("h_same_event_stats"))->GetXaxis()->SetBinLabel(5, "Total j-h pairs with accpeted");
      registry.add("h_mixjeth_event_stats", "Mixed event statistics; Event pair type; counts", {HistType::kTH1F, {{10, 0.5, 10.5}}});
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(1, "Total mixed events");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(2, "Total jets");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(3, "Total jets with cuts");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(4, "Total j-h pairs");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(5, "Total j-h pairs with accpeted");
    }

    if (doprocessHFJetCorrelation) {
      registry.add("h_d0jet_pt", "D0 jet pT;#it{p}_{T,jet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_d0jet_corrpt", "D0 jet pT;#it{p}_{T,jet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_d0jet_eta", "D0 jet eta;#eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_d0jet_phi", "D0 jet phi;#phi; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_d0_pt", ";p_{T,D^{0}};dN/dp_{T,D^{0}}", {HistType::kTH1F, {{200, 0., 10.}}});
      registry.add("h_d0_mass", ";m_{D^{0}} (GeV/c^{2});dN/dm_{D^{0}}", {HistType::kTH1F, {{1000, 0., 10.}}});
      registry.add("h_d0_eta", ";#eta_{D^{0}} (GeV/c^{2});dN/d#eta_{D^{0}}", {HistType::kTH1F, {{200, -5., 5.}}});
      registry.add("h_d0_phi", ";#varphi_{D^{0}} (GeV/c^{2});dN/d#varphi_{D^{0}}", {HistType::kTH1F, {{200, -10., 10.}}});
      registry.add("h2_d0jet_detadphi", "D^{0}-jets deta vs dphi; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
    }

    if (doprocessMixLeadingJetHadronCorrelation || doprocessSpectraAreaSubMCD) {
      registry.add("h_mixdijet_pair_counts_cut", "ME: number of pairs with leadingjet & subleadingjet cut pair; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
      registry.add("h_mixdijet_dphi", "mixed event: dijet #Delta#varphi before converted to 0-2pi; #Delta#varphi; counts", {HistType::kTH1F, {{126, -6.3, 6.3}}});
      registry.add("h_mixleadjet_corrpt", "mixed event: leading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_mixsubleadjet_corrpt", "mixed event: subleading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_mixleadjet_eta", "mixed event: leading jet eta; #eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_mixsubleadjet_eta", "mixed event: subleading jet eta; #eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h2_mixdijet_detanoflip_dphi", "mixed event: dijet #Delta#eta no flip vs #Delta#varphi; #Delta#eta_{noflip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_mixdijet_deta_dphi", "mixed event: dijet #Delta#eta flip vs #Delta#varphi; #Delta#eta_{flip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_mixdijet_Asymmetry", "mixed event: dijet Asymmetry; #it{p}_{T,subleadingjet} (GeV/#it{c}); #it{X}_{J}; counts", {HistType::kTH2F, {jetPtAxisRhoAreaSub, {40, 0, 1.0}}});

      registry.add("h_mixjeth_detatot", "mixed event: jet-hadron correlations; no flip #Delta#eta", {HistType::kTH1F, {detaAxis}});
      registry.add("h_mixjeth_deta", "mixed event: jet-hadron correlations; #Delta#eta", {HistType::kTH1F, {detaAxis}});
      registry.add("h_mixjeth_dphi", "mixed event: jet-hadron correlations; #Delta#phi", {HistType::kTH1F, {dphiAxis}});
      registry.add("h2_mixjeth_detatot_dphi", "mixed event: jet-hadron correlations; no flip #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_mixjeth_deta_dphi", "mixed event: jet-hadron correlations; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("thn_mixjethadron", "mixed event: jet-h correlations; leadingJetPt; subleadingJetPt; trackPt; no flip jeth#Delta#eta; #Delta#eta_{jet1,2}; jeth#Delta#eta; jeth#Delta#phi; poolBin",
                   HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, detaAxis, detaAxis, dphiAxis, {10, 0, 10}});

      registry.add("h_mix_event_stats", "Mixed event statistics; Event pair type; counts", {HistType::kTH1F, {{10, 0.5, 10.5}}});
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(1, "Total mixed events");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(2, "Total dijets");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(3, "Total dijets with cuts");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(4, "Total Lj-h pairs");
      registry.get<TH1>(HIST("h_mix_event_stats"))->GetXaxis()->SetBinLabel(5, "Total Lj-h pairs with cut");
    }

    if (doprocessSpectraMCP || doprocessSpectraMCPWeighted) {
      registry.add("h_mcColl_counts", " number of mc events; event status; entries", {HistType::kTH1F, {{10, 0, 10}}});
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(1, "allMcColl");
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(2, "vertexZ");
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(3, "noRecoColl");
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(4, "recoEvtSel");
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(5, "centralitycut");
      registry.get<TH1>(HIST("h_mcColl_counts"))->GetXaxis()->SetBinLabel(6, "occupancycut");

      registry.add("h_mc_zvertex", "position of collision ;#it{Z} (cm)", {HistType::kTH1F, {{300, -15.0, 15.0}}});
      registry.add("h_mc_mult", " multiplicity globle tracks; entries", {HistType::kTH1F, {{3000, 0, 60000}}});
      registry.add("h_jet_pt_part", "partvjet pT;#it{p}_{T,jet}^{part} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_jet_eta_part", "part jet #eta;#eta^{part}; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_jet_phi_part", "part jet #varphi;#phi^{part}; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_jet_area_part", "part jet Area_{jet}; Area_{jet}^{part}; counts", {HistType::kTH1F, {{150, 0., 1.5}}});
      registry.add("h_ntracks_part", "part jet N_{jet tracks}; N_{jet, tracks}^{part}; counts", {HistType::kTH1F, {{200, -0.5, 199.5}}});
      registry.add("h2_jet_pt_part_track_pt_part", "part jet #it{p}_{T,jet} vs. #it{p}_{T,track}; #it{p}_{T,jet}^{part} (GeV/#it{c}); #it{p}_{T,track}^{part} (GeV/#it{c})", {HistType::kTH2F, {jetPtAxisRhoAreaSub, trackPtAxis}});
      if (doprocessSpectraMCPWeighted) {
        registry.add("h_mcColl_counts_weight", " number of weighted mc events; event status; entries", {HistType::kTH1F, {{10, 0, 10}}});
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(1, "allMcColl");
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(2, "vertexZ");
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(3, "noRecoColl");
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(4, "recoEvtSel");
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(5, "centralitycut");
        registry.get<TH1>(HIST("h_mcColl_counts_weight"))->GetXaxis()->SetBinLabel(6, "occupancycut");
        registry.add("h2_jet_ptcut_part", "p_{T} cut;p_{T,jet}^{part} (GeV/#it{c});N;entries", {HistType::kTH2F, {{300, 0, 300}, {20, 0, 5}}});
        registry.add("h_jet_phat_part_weighted", "jet #hat{p};#hat{p} (GeV/#it{c});entries", {HistType::kTH1F, {{1000, 0, 1000}}});
      }
    }

    if (doprocessSpectraAreaSubMCP) {
      registry.add("h_mcColl_counts_areasub", " number of mc events; event status; entries", {HistType::kTH1F, {{10, 0, 10}}});
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(1, "allMcColl");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(2, "vertexZ");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(3, "noRecoColl");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(4, "splitColl");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(5, "recoEvtSel");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(6, "centralitycut");
      registry.get<TH1>(HIST("h_mcColl_counts_areasub"))->GetXaxis()->SetBinLabel(7, "occupancycut");

      registry.add("h_mcColl_rho", "mc collision rho;#rho (GeV/#it{c}); counts", {HistType::kTH1F, {{500, 0.0, 500.0}}});
      registry.add("h_mcColl_centrality", "mc collision centrality; centrality; counts", {HistType::kTH1F, {centralityAxis}});
      registry.add("h_jet_pt_part_rhoareasubtracted", "part jet corr pT;#it{p}_{T,jet}^{part} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_jet_eta_part_rhoareasubtracted", "part jet #eta;#eta^{part}; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_jet_phi_part_rhoareasubtracted", "part jet #varphi;#varphi^{part}; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_jet_area_part_rhoareasubtracted", "part jet Area_{jet}; Area_{jet}^{part}; counts", {HistType::kTH1F, {{150, 0., 1.5}}});
      registry.add("h_ntracks_part_rhoareasubtracted", "part jet N_{jet tracks}; N_{jet, tracks}^{part}; counts", {HistType::kTH1F, {{200, -0.5, 199.5}}});

      //========jet-hadron correlations======================
      registry.add("h_dijet_pair_counts_part", "MCP number of paris with good leading-subleading jets; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
      registry.add("h_dijet_pair_counts_cut_part", "MCP number of pairs with leadingjet & subleadingjet cut pair; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
      registry.add("h_leadjet_pt_part", "MCP leading jet pT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_leadjet_corrpt_part", "MCP leading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_leadjet_eta_part", "MCP leading jet eta;#eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_leadjet_phi_part", "MCP leading jet phi;#phi; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h_subleadjet_pt_part", "MCP subleading jet pT;#it{p}_{T,subleadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxis}});
      registry.add("h_subleadjet_corrpt_part", "MCP subleading jet corrpT; #it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_subleadjet_eta_part", "MCP subleading jet eta;#eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_subleadjet_phi_part", "MCP subleading jet phi;#phi; counts", {HistType::kTH1F, {phiAxis}});
      registry.add("h2_dijet_detanoflip_dphi_part", "MCP dijet #Delta#eta no flip vs #Delta#varphi; #Delta#eta_{noflip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_dijet_deta_dphi_part", "MCP dijet #Delta#eta flip vs #Delta#varphi; #Delta#eta_{flip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_dijet_Asymmetry_part", "MCP: dijet Asymmetry; #it{p}_{T,subleadingjet} (GeV/#it{c}); #it{X}_{J}; counts", {HistType::kTH2F, {jetPtAxisRhoAreaSub, {40, 0, 1.0}}});

      registry.add("h_jeth_detatot_part", "MCP jet-hadron tot #Delta#eta;#Delta#eta; counts", {HistType::kTH1F, {detaAxis}});
      registry.add("h_jeth_deta_part", "MCP jet-hadron #Delta#eta;#Delta#eta; counts", {HistType::kTH1F, {detaAxis}});
      registry.add("h_jeth_dphi_part", "MCP jet-hadron #Delta#varphi;#Delta#varphi; counts", {HistType::kTH1F, {dphiAxis}});
      registry.add("h2_jeth_detatot_dphi_part", "MCP jeth deta vs dphi with physical cuts; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_deta_dphi_part", "MCP jeth deta vs dphi; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_physicalcutsup_deta_dphi_part", "MCP jeth deta vs dphi with physical cuts |#Delta#eta_{jet}| > 1.0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_physicalcutsmd_deta_dphi_part", "MCP jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| #in (0.5, 1.0); #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_physicalcutsdw_deta_dphi_part", "MCP jeth deta vs dphi with physical cuts  |#Delta#eta_{jet1,2}| < 0.5; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_physicalcutsHup_deta_dphi_part", "MCP jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| > 0.5,#Delta#eta_{jet2}>0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_jeth_physicalcutsHdw_deta_dphi_part", "MCP jeth deta vs dphi with physical cuts |#Delta#eta_{jet1,2}| < 0.5 #Delta#eta_{jet2}>0; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("thn_jethadron_correlationsi_part", "MCP jet-h correlations; leadingjetpT; subleadingjetpT; trackpT; no flip jeth#Delta#eta; #Delta#eta_{jet1,2}; jeth#Delta#eta; jeth#Delta#varphi", HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, detaAxis, detaAxis, dphiAxis});

      registry.add("h_mixmc_dijet_pair_counts_cut", "ME: number of pairs with leadingjet & subleadingjet cut pair; jet pairs; counts", {HistType::kTH1F, {{10, 0, 10}}});
      registry.add("h_mixmc_dijet_dphi", "mixed event: dijet #Delta#varphi before converted to 0-2pi; #Delta#varphi; counts", {HistType::kTH1F, {{126, -6.3, 6.3}}});
      registry.add("h_mixmc_leadjet_corrpt", "mixed event: leading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_mixmc_subleadjet_corrpt", "mixed event: subleading jet corrpT;#it{p}_{T,leadingjet} (GeV/#it{c}); counts", {HistType::kTH1F, {jetPtAxisRhoAreaSub}});
      registry.add("h_mixmc_leadjet_eta", "mixed event: leading jet eta; #eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h_mixmc_subleadjet_eta", "mixed event: subleading jet eta; #eta; counts", {HistType::kTH1F, {etaAxis}});
      registry.add("h2_mixmc_dijet_detanoflip_dphi", "mixed event: dijet #Delta#eta no flip vs #Delta#varphi; #Delta#eta_{noflip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_mixmc_dijet_deta_dphi", "mixed event: dijet #Delta#eta flip vs #Delta#varphi; #Delta#eta_{flip}; #Delta#varphi; counts", {HistType::kTH2F, {detaAxis, {63, 0, 6.3}}});
      registry.add("h2_mixmc_dijet_Asymmetry", "mixed event: dijet Asymmetry; #it{p}_{T,subleadingjet} (GeV/#it{c}); #it{X}_{J}; counts", {HistType::kTH2F, {jetPtAxisRhoAreaSub, {40, 0, 1.0}}});

      registry.add("h_mixmc_jeth_detatot", "mixed event: jet-hadron correlations; no flip #Delta#eta", {HistType::kTH1F, {detaAxis}});
      registry.add("h_mixmc_jeth_deta", "mixed event: jet-hadron correlations; #Delta#eta", {HistType::kTH1F, {detaAxis}});
      registry.add("h_mixmc_jeth_dphi", "mixed event: jet-hadron correlations; #Delta#phi", {HistType::kTH1F, {dphiAxis}});
      registry.add("h2_mixmc_jeth_detatot_dphi", "mixed event: jet-hadron correlations; no flip #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("h2_mixmc_jeth_deta_dphi", "mixed event: jet-hadron correlations; #Delta#eta; #Delta#phi", {HistType::kTH2F, {detaAxis, dphiAxis}});
      registry.add("thn_mixmc_jethadron", "mixed event: jet-h correlations; leadingJetPt; subleadingJetPt; trackPt; no flip jeth#Delta#eta; #Delta#eta_{jet1,2}; jeth#Delta#eta; jeth#Delta#phi; poolBin",
                   HistType::kTHnSparseF, {jetPtAxisRhoAreaSub, jetPtAxisRhoAreaSub, trackPtAxis, detaAxis, detaAxis, detaAxis, dphiAxis, {10, 0, 10}});

      registry.add("h_mixmc_event_stats", "Mixed event statistics; Event pair type; counts", {HistType::kTH1F, {{10, 0.5, 10.5}}});
      registry.get<TH1>(HIST("h_mixmc_event_stats"))->GetXaxis()->SetBinLabel(1, "Total mixed events");
      registry.get<TH1>(HIST("h_mixmc_event_stats"))->GetXaxis()->SetBinLabel(2, "Total dijets");
      registry.get<TH1>(HIST("h_mixmc_event_stats"))->GetXaxis()->SetBinLabel(3, "Total dijets with cuts");
      registry.get<TH1>(HIST("h_mixmc_event_stats"))->GetXaxis()->SetBinLabel(4, "Total Lj-h pairs");
      registry.get<TH1>(HIST("h_mixmc_event_stats"))->GetXaxis()->SetBinLabel(5, "Total Lj-h pairs with cut");
    }
  }

  template <typename TTracks, typename TJets>
  bool isAcceptedJet(TJets const& jet, bool mcLevelIsParticleLevel = false)
  {
    double jetAreaLimit = -98.0;
    double constituentPtMin = -98.0;
    double constituentPtMax = 9998.0;
    if (jetAreaFractionMin > jetAreaLimit) {
      if (jet.area() < jetAreaFractionMin * o2::constants::math::PI * (jet.r() / 100.0) * (jet.r() / 100.0)) {
        return false;
      }
    }
    bool checkConstituentPt = true;
    bool checkConstituentMinPt = (leadingConstituentPtMin > constituentPtMin);
    bool checkConstituentMaxPt = (leadingConstituentPtMax < constituentPtMax);
    if (!checkConstituentMinPt && !checkConstituentMaxPt) {
      checkConstituentPt = false;
    }
    if (mcLevelIsParticleLevel && !checkLeadConstituentPtForMcpJets) {
      checkConstituentPt = false;
    }

    if (checkConstituentPt) {
      bool isMinLeadingConstituent = !checkConstituentMinPt;
      bool isMaxLeadingConstituent = true;

      for (const auto& constituent : jet.template tracks_as<TTracks>()) {
        double pt = constituent.pt();

        if (checkConstituentMinPt && pt >= leadingConstituentPtMin) {
          isMinLeadingConstituent = true;
        }
        if (checkConstituentMaxPt && pt > leadingConstituentPtMax) {
          isMaxLeadingConstituent = false;
        }
      }
      return isMinLeadingConstituent && isMaxLeadingConstituent;
    }
    return true;
  }

  template <typename TJets>
  void fillJetHistograms(TJets const& jet, float weight = 1.0)
  {
    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCD * pTHat || pTHat < pTHatAbsoluteMin) {
      return;
    }
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      registry.fill(HIST("h_jet_pt"), jet.pt(), weight);
      registry.fill(HIST("h_jet_eta"), jet.eta(), weight);
      registry.fill(HIST("h_jet_phi"), jet.phi(), weight);
      registry.fill(HIST("h_jet_area"), jet.area(), weight);
      registry.fill(HIST("h_jet_ntracks"), jet.tracksIds().size(), weight);
      //      registry.fill(HIST("h3_jet_eta_phi_pt"), jet.eta(), jet.phi(), jet.pt(), weight);
      // registry.fill(HIST("h2_jet_pt_jet_ntracks"), jet.pt(), jet.tracksIds().size(), weight);
    }

    for (const auto& constituent : jet.template tracks_as<aod::JetTracks>()) {
      registry.fill(HIST("h2_jet_pt_track_pt"), jet.pt(), constituent.pt(), weight);
    }
  }

  template <typename TJets>
  void fillJetAreaSubHistograms(TJets const& jet, float rho, float weight = 1.0)
  {
    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCD * pTHat || pTHat < pTHatAbsoluteMin) {
      return;
    }
    double jetcorrpt = jet.pt() - (rho * jet.area());
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      // fill jet histograms after area-based subtraction
      registry.fill(HIST("h_jet_pt_rhoareasubtracted"), jetcorrpt, weight);
      if (jetcorrpt > 0) {
        registry.fill(HIST("h_jet_eta_rhoareasubtracted"), jet.eta(), weight);
        registry.fill(HIST("h_jet_phi_rhoareasubtracted"), jet.phi(), weight);
        registry.fill(HIST("h_jet_area_rhoareasubtracted"), jet.area(), weight);
        registry.fill(HIST("h_jet_ntracks_rhoareasubtracted"), jet.tracksIds().size(), weight);
        //        registry.fill(HIST("h3_jet_eta_phi_pt_rhoareasubtracted"), jet.eta(), jet.phi(), jetcorrpt, weight);
      }
    }
  }

  template <typename TJets>
  void fillMCPHistograms(TJets const& jet, float weight = 1.0)
  {
    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCP * pTHat || pTHat < pTHatAbsoluteMin) {
      return;
    }
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      // fill mcp jet histograms
      registry.fill(HIST("h_jet_pt_part"), jet.pt(), weight);
      registry.fill(HIST("h_jet_eta_part"), jet.eta(), weight);
      registry.fill(HIST("h_jet_phi_part"), jet.phi(), weight);
      registry.fill(HIST("h_jet_area_part"), jet.area(), weight);
      registry.fill(HIST("h_jet_ntracks_part"), jet.tracksIds().size(), weight);
    }

    for (const auto& constituent : jet.template tracks_as<aod::JetParticles>()) {
      registry.fill(HIST("h2_jet_pt_part_track_pt_part"), jet.pt(), constituent.pt(), weight);
    }
  }

  template <typename TJets>
  void fillMCPAreaSubHistograms(TJets const& jet, float rho = 0.0, float weight = 1.0)
  {
    float pTHat = 10. / (std::pow(weight, 1.0 / pTHatExponent));
    if (jet.pt() > pTHatMaxMCP * pTHat || pTHat < pTHatAbsoluteMin) {
      return;
    }
    if (jet.r() == round(selectedJetsRadius * 100.0f)) {
      // fill mcp jet histograms
      double jetcorrpt = jet.pt() - (rho * jet.area());
      registry.fill(HIST("h_jet_pt_part_rhoareasubtracted"), jetcorrpt, weight);
      if (jetcorrpt > 0) {
        registry.fill(HIST("h_jet_eta_part_rhoareasubtracted"), jet.eta(), weight);
        registry.fill(HIST("h_jet_phi_part_rhoareasubtracted"), jet.phi(), weight);
        registry.fill(HIST("h_jet_area_part_rhoareasubtracted"), jet.area(), weight);
        registry.fill(HIST("h_jet_ntracks_part_rhoareasubtracted"), jet.tracksIds().size(), weight);
      }
    }
  }

  template <typename TTracks>
  void fillTrackHistograms(TTracks const& track, float weight = 1.0)
  {
    registry.fill(HIST("h_track_pt"), track.pt(), weight);
    registry.fill(HIST("h2_track_eta_track_phi"), track.eta(), track.phi(), weight);
    //    registry.fill(HIST("h3_track_eta_phi_pt"), track.eta(), track.phi(), track.pt(), weight);
  }

  template <typename TCollision, typename TJets, typename TTracks>
  void fillJetHadronHistograms(const TCollision& collision, const TJets& jets, const TTracks& tracks, float eventWeight = 1.0)
  {
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      registry.fill(HIST("h_jeth_event_stats"), 2);
      double ptCorr = jet.pt() - jet.area() * collision.rho();
      if (ptCorr < subleadingjetptMin) {
        continue;
      }
      registry.fill(HIST("h_jeth_event_stats"), 3);
      for (auto const& track : tracks) {
        registry.fill(HIST("h_jeth_event_stats"), 4);
        if (!jetderiveddatautilities::selectTrack(track, trackSelection)) {
          continue;
        }
        registry.fill(HIST("h_jeth_event_stats"), 5);
        double deta = track.eta() - jet.eta();
        double dphi = track.phi() - jet.phi();
	dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);
	double dr = std::sqrt(deta * deta + dphi * dphi);
        registry.fill(HIST("thn_jeth_correlations"), ptCorr, track.pt(), deta, dphi, dr);
      }
    }
  }

  template <typename TCollisions, typename TJets, typename TTracks>
  void fillMixJetHadronHistograms(const TCollisions& collisions, const TJets& jets, const TTracks& tracks, float eventWeight = 1.0)
  {
    auto tracksTuple = std::make_tuple(jets, tracks);
    Pair<TCollisions, TJets, TTracks, BinningType> pairData{corrBinning, numberEventsMixed, -1, collisions, tracksTuple, &cache};

    for (const auto& [c1, jets1, c2, tracks2] : pairData) {
      registry.fill(HIST("h_mixjeth_event_stats"), 1);
      int poolBin = corrBinning.getBin(std::make_tuple(c2.posZ(), c2.multFT0M()));

      for (auto const& jet : jets1) {
        if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
          continue;
        }
        using TracksTable = std::decay_t<decltype(tracks)>;
        if (!isAcceptedJet<TracksTable>(jet)) {
          continue;
        }
        registry.fill(HIST("h_mixjeth_event_stats"), 2);
        double ptCorr = jet.pt() - jet.area() * c1.rho();
        if (ptCorr < ptLeadingCorr) {
          continue;
        }
        registry.fill(HIST("h_mixjeth_event_stats"), 3);
        for (auto const& track : tracks2) {
          registry.fill(HIST("h_mixjeth_event_stats"), 4);
          if (!jetderiveddatautilities::selectTrack(track, trackSelection))
            continue;
          registry.fill(HIST("h_mixjeth_event_stats"), 5);
          double deta = track.eta() - jet.eta();
          double dphi = track.phi() - jet.phi();
          dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);
	  double dr = std::sqrt(deta * deta + dphi * dphi);
          registry.fill(HIST("thn_mixjeth_correlations"), ptCorr, track.pt(), deta, dphi, dr);
        }
      }
    }
  }

  template <typename TCollision, typename TJets, typename TTracks>
  void fillLeadingJetHadronHistograms(const TCollision& collision, const TJets& jets, const TTracks& tracks, float eventWeight = 1.0)
  {
    registry.fill(HIST("h_centrality"), collision.centFT0M());

    const double trackJethCut = 2.001;
    const double etaGapdw = 0.5;
    const double etaGapup = 1.0;
    using JetType = std::decay_t<decltype(*jets.begin())>;
    std::optional<JetType> leadingJet;
    std::optional<JetType> subleadingJet;
    double ptLeadingCorr = -1.0;
    double ptSubleadingCorr = -1.0;

    //====Step 1: find leading and subleading jets ============
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      using TracksTable = std::decay_t<decltype(tracks)>;
      if (!isAcceptedJet<TracksTable>(jet)) {
        continue;
      }

      double ptCorr = jet.pt() - jet.area() * collision.rho();
      registry.fill(HIST("h_inclusivejet_corrpt"), ptCorr, eventWeight);
      if (ptCorr > ptLeadingCorr) {
        subleadingJet = leadingJet;
        ptSubleadingCorr = ptLeadingCorr;
        leadingJet = jet;
        ptLeadingCorr = ptCorr;
      } else if (ptCorr > ptSubleadingCorr) {
        subleadingJet = jet;
        ptSubleadingCorr = ptCorr;
      }
    }
    if (!leadingJet || !subleadingJet) {
      return; // skip if fewer than 2 jets
    }
    double deltaPhiJets = leadingJet->phi() - subleadingJet->phi();
    if (std::abs(deltaPhiJets) < o2::constants::math::PIHalf) {
      return;
    }
    registry.fill(HIST("h_dijet_dphi"), deltaPhiJets, eventWeight);
    deltaPhiJets = RecoDecay::constrainAngle(deltaPhiJets, 0.);

    // === Step2: eta ordering etajet1 > etajet2) ===
    double etaJet1Raw = leadingJet->eta();
    double etaJet2Raw = subleadingJet->eta();
    // double multeta1eta2 = etaJet1Raw * etaJet2Raw;
    double deltaEtaJetsNoflip = etaJet1Raw - etaJet2Raw;
    double flip = (etaJet1Raw > etaJet2Raw) ? 1.0 : -1.0;
    //    double flip = (etaJet1Raw > 0) ? 1.0 : -1.0;    //Dr.Yang suggestion
    double etajet1 = flip * etaJet1Raw;      // leading jet eta after flip
    double etajet2 = flip * etaJet2Raw;      // subleading jet eta after flip
    double deltaEtaJets = etajet1 - etajet2; // >= 0

    registry.fill(HIST("h_dijet_pair_counts"), 1);
    registry.fill(HIST("h_leadjet_pt"), leadingJet->pt(), eventWeight);
    registry.fill(HIST("h_subleadjet_pt"), subleadingJet->pt(), eventWeight);
    registry.fill(HIST("h_leadjet_corrpt"), ptLeadingCorr, eventWeight);
    registry.fill(HIST("h_subleadjet_corrpt"), ptSubleadingCorr, eventWeight);

    if (ptLeadingCorr > leadingjetptMin && ptSubleadingCorr > subleadingjetptMin) {
      registry.fill(HIST("h_dijet_pair_counts_cut"), 2);
      registry.fill(HIST("h_leadjet_eta"), etaJet1Raw, eventWeight);
      registry.fill(HIST("h_subleadjet_eta"), etaJet2Raw, eventWeight);
      registry.fill(HIST("h_leadjet_phi"), leadingJet->phi(), eventWeight);
      registry.fill(HIST("h_subleadjet_phi"), subleadingJet->phi(), eventWeight);
      registry.fill(HIST("h2_dijet_detanoflip_dphi"), deltaEtaJetsNoflip, deltaPhiJets, eventWeight);
      registry.fill(HIST("h2_dijet_deta_dphi"), deltaEtaJets, deltaPhiJets, eventWeight);
      registry.fill(HIST("h2_dijet_Asymmetry"), ptSubleadingCorr, ptSubleadingCorr / ptLeadingCorr, eventWeight);

      for (auto const& track : tracks) {
        if (!jetderiveddatautilities::selectTrack(track, trackSelection)) {
          continue;
        }
        double detatot = track.eta() - etaJet1Raw;
        double deta = flip * (track.eta() - etaJet1Raw); // always relative to leadingJet (after flip)
        //	double deta = flip * (track.eta());  // Dr.Yang
        double dphi = track.phi() - leadingJet->phi();
//        dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf, 1.5 * o2::constants::math::PI);
        dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);

        registry.fill(HIST("h_jeth_detatot"), detatot, eventWeight);
        registry.fill(HIST("h_jeth_deta"), deta, eventWeight);
        registry.fill(HIST("h_jeth_dphi"), dphi, eventWeight);
        registry.fill(HIST("h2_jeth_detatot_dphi"), detatot, dphi, eventWeight);
        registry.fill(HIST("h2_jeth_deta_dphi"), deta, dphi, eventWeight);
        registry.fill(HIST("thn_jethadron_correlations"), ptLeadingCorr, ptSubleadingCorr, track.pt(), detatot, deltaEtaJets, deta, dphi, eventWeight);
        if (track.pt() < trackJethCut) {
          if (std::abs(deltaEtaJets) >= etaGapup)
            registry.fill(HIST("h2_jeth_physicalcutsup_deta_dphi"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) >= etaGapdw && std::abs(deltaEtaJets) < etaGapup)
            registry.fill(HIST("h2_jeth_physicalcutsmd_deta_dphi"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) < etaGapdw)
            registry.fill(HIST("h2_jeth_physicalcutsdw_deta_dphi"), deta, dphi, eventWeight);
          //	   if(multeta1eta2 > 0) registry.fill(HIST("h2_jeth_physicalcutsHup_deta_dphi"), deta, dphi, eventWeight); //Dr.Yang
          //	   if(multeta1eta2 < 0) registry.fill(HIST("h2_jeth_physicalcutsHdw_deta_dphi"), deta, dphi, eventWeight);//Dr.Yang
        }
      }
    }
  }

  //....below....mixed events 20250918.................................
  template <typename TCollisions, typename TJets, typename TTracks>
  void fillMixLeadingJetHadronHistograms(const TCollisions& collisions, const TJets& jets, const TTracks& tracks, float eventWeight = 1.0)
  {
    auto tracksTuple = std::make_tuple(jets, tracks);
    Pair<TCollisions, TJets, TTracks, BinningType> pairData{corrBinning, numberEventsMixed, -1, collisions, tracksTuple, &cache};

    int totalmix = 0;
    int totaldijets = 0;
    int totaldijetscut = 0;
    int totalPairs = 0;
    int passedPairs = 0;

    for (const auto& [c1, jets1, c2, tracks2] : pairData) {
      totalmix++;
      registry.fill(HIST("h_mix_event_stats"), 1);
      int poolBin = corrBinning.getBin(std::make_tuple(c2.posZ(), c2.multFT0M()));

      using JetType = std::decay_t<decltype(*jets.begin())>;
      std::optional<JetType> leadingJet;
      std::optional<JetType> subleadingJet;
      double ptLeadingCorr = -1.0;
      double ptSubleadingCorr = -1.0;

      for (auto const& jet : jets1) {
        if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
          continue;
        }
        /*   if (!isAcceptedJet<aod::JetTracks>(jet)) {
           continue; // for pp Reason: Trying to dereference index with a wrong type in tracks_as<T> for base target "JTracks"
           }*/
        using TracksTable = std::decay_t<decltype(tracks)>;
        if (!isAcceptedJet<TracksTable>(jet)) {
          continue;
        }

        double ptCorr = jet.pt() - jet.area() * c1.rho();
        if (ptCorr > ptLeadingCorr) {
          subleadingJet = leadingJet;
          ptSubleadingCorr = ptLeadingCorr;
          leadingJet = jet;
          ptLeadingCorr = ptCorr;
        } else if (ptCorr > ptSubleadingCorr) {
          subleadingJet = jet;
          ptSubleadingCorr = ptCorr;
        }
      }

      if (!leadingJet || !subleadingJet) {
        return;
      }
      double deltaPhiJets = leadingJet->phi() - subleadingJet->phi();
      if (std::abs(deltaPhiJets) < o2::constants::math::PIHalf) {
        return;
      }
      registry.fill(HIST("h_mixdijet_dphi"), deltaPhiJets, eventWeight);
      deltaPhiJets = RecoDecay::constrainAngle(deltaPhiJets, 0.);
      totaldijets++;
      registry.fill(HIST("h_mix_event_stats"), 2);

      double flip = 1.0;
      double etaJet1Raw = leadingJet->eta();
      double etaJet2Raw = subleadingJet->eta();
      flip = (etaJet1Raw > etaJet2Raw) ? 1.0 : -1.0;
      double etajet1 = flip * etaJet1Raw;
      double etajet2 = flip * etaJet2Raw;
      double deltaEtaJetsNoflip = etaJet1Raw - etaJet2Raw;
      double deltaEtaJets = etajet1 - etajet2;

      registry.fill(HIST("h_mixleadjet_corrpt"), ptLeadingCorr, eventWeight);
      registry.fill(HIST("h_mixsubleadjet_corrpt"), ptSubleadingCorr, eventWeight);

      if (ptLeadingCorr > leadingjetptMin && ptSubleadingCorr > subleadingjetptMin) {
        totaldijetscut++;
        registry.fill(HIST("h_mix_event_stats"), 3);
        registry.fill(HIST("h_mixdijet_pair_counts_cut"), 2);

        registry.fill(HIST("h_mixleadjet_eta"), etaJet1Raw, eventWeight);
        registry.fill(HIST("h_mixsubleadjet_eta"), etaJet2Raw, eventWeight);
        registry.fill(HIST("h2_mixdijet_detanoflip_dphi"), deltaEtaJetsNoflip, deltaPhiJets, eventWeight);
        registry.fill(HIST("h2_mixdijet_deta_dphi"), deltaEtaJets, deltaPhiJets, eventWeight);
        registry.fill(HIST("h2_mixdijet_Asymmetry"), ptSubleadingCorr, ptSubleadingCorr / ptLeadingCorr, eventWeight);

        for (auto const& track : tracks2) {
          totalPairs++;
          registry.fill(HIST("h_mix_event_stats"), 4);
          if (!jetderiveddatautilities::selectTrack(track, trackSelection))
            continue;
          passedPairs++;
          registry.fill(HIST("h_mix_event_stats"), 5);
          double detatot = track.eta() - etaJet1Raw;
          double deta = flip * (track.eta() - etajet1);
          double dphi = track.phi() - leadingJet->phi();
          dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);

          registry.fill(HIST("h_mixjeth_detatot"), detatot, eventWeight);
          registry.fill(HIST("h_mixjeth_deta"), deta, eventWeight);
          registry.fill(HIST("h_mixjeth_dphi"), dphi, eventWeight);
          registry.fill(HIST("h2_mixjeth_detatot_dphi"), detatot, dphi, eventWeight);
          registry.fill(HIST("h2_mixjeth_deta_dphi"), deta, dphi, eventWeight);
          registry.fill(HIST("thn_mixjethadron"), ptLeadingCorr, ptSubleadingCorr, track.pt(), detatot, deltaEtaJets, deta, dphi, poolBin, eventWeight);
        }
      }
    }
    registry.fill(HIST("h_mix_event_stats"), 6, totalmix);
    registry.fill(HIST("h_mix_event_stats"), 7, totaldijets);
    registry.fill(HIST("h_mix_event_stats"), 8, totaldijetscut);
    registry.fill(HIST("h_mix_event_stats"), 9, totalPairs);
    registry.fill(HIST("h_mix_event_stats"), 10, passedPairs);
  }
  //..........up mixed events..........................................

  template <typename TmcCollision, typename TJets, typename TParticles>
  void fillMCPLeadingJetHadronHistograms(const TmcCollision& mccollision, const TJets& jets, const TParticles& particles, float eventWeight = 1.0)
  {
    const double particleJethCut = 2.001;
    const double etaGapdw = 0.5;
    const double etaGapup = 1.0;
    using JetType = std::decay_t<decltype(*jets.begin())>;
    std::optional<JetType> leadingJet;
    std::optional<JetType> subleadingJet;
    double ptLeadingCorr = -1.0;
    double ptSubleadingCorr = -1.0;
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      /* if (!isAcceptedJet<aod::JetParticles>(jet, true))  {
       continue;
       }*/
      using ParticlesTable = std::decay_t<decltype(particles)>;
      if (!isAcceptedJet<ParticlesTable>(jet, true)) {
        continue;
      }

      double ptCorr = jet.pt() - jet.area() * mccollision.rho();
      if (ptCorr > ptLeadingCorr) {
        subleadingJet = leadingJet;
        ptSubleadingCorr = ptLeadingCorr;
        leadingJet = jet;
        ptLeadingCorr = ptCorr;
      } else if (ptCorr > ptSubleadingCorr) {
        subleadingJet = jet;
        ptSubleadingCorr = ptCorr;
      }
    }

    if (!leadingJet || !subleadingJet) {
      return; // skip if fewer than 2 jets
    }
    double deltaPhiJets = leadingJet->phi() - subleadingJet->phi();
    if (std::abs(deltaPhiJets) < o2::constants::math::PIHalf) {
      return; // require
    }
    deltaPhiJets = RecoDecay::constrainAngle(deltaPhiJets, 0.);

    // === Step2: eta ordering etajet1 > etajet2) ===
    double etaJet1Raw = leadingJet->eta();
    double etaJet2Raw = subleadingJet->eta();
    // double multeta1eta2 = etaJet1Raw * etaJet2Raw;
    double deltaEtaJetsNoflip = etaJet1Raw - etaJet2Raw;
    double flip = (etaJet1Raw > etaJet2Raw) ? 1.0 : -1.0;
    double etajet1 = flip * etaJet1Raw;      // leading jet eta after lip
    double etajet2 = flip * etaJet2Raw;      // subleading jet eta after flip
    double deltaEtaJets = etajet1 - etajet2; // >= 0

    registry.fill(HIST("h_dijet_pair_counts_part"), 1);
    registry.fill(HIST("h_leadjet_pt_part"), leadingJet->pt(), eventWeight);
    registry.fill(HIST("h_subleadjet_pt_part"), subleadingJet->pt(), eventWeight);
    registry.fill(HIST("h_leadjet_corrpt_part"), ptLeadingCorr, eventWeight);
    registry.fill(HIST("h_subleadjet_corrpt_part"), ptSubleadingCorr, eventWeight);

    if (ptLeadingCorr > leadingjetptMin && ptSubleadingCorr > subleadingjetptMin) {
      registry.fill(HIST("h_dijet_pair_counts_cut_part"), 2);
      registry.fill(HIST("h_leadjet_eta_part"), etaJet1Raw, eventWeight);
      registry.fill(HIST("h_subleadjet_eta_part"), etaJet2Raw, eventWeight);
      registry.fill(HIST("h_leadjet_phi_part"), leadingJet->phi(), eventWeight);
      registry.fill(HIST("h_subleadjet_phi_part"), subleadingJet->phi(), eventWeight);
      registry.fill(HIST("h2_dijet_detanoflip_dphi_part"), deltaEtaJetsNoflip, deltaPhiJets, eventWeight);
      registry.fill(HIST("h2_dijet_deta_dphi_part"), deltaEtaJets, deltaPhiJets, eventWeight);
      registry.fill(HIST("h2_dijet_Asymmetry_part"), ptSubleadingCorr, ptSubleadingCorr / ptLeadingCorr, eventWeight);

      for (auto const& particle : particles) {
        double detatot = particle.eta() - etaJet1Raw;
        double deta = flip * (particle.eta() - etaJet1Raw); // always relative to leadingJet (after flip)
        double dphi = particle.phi() - leadingJet->phi();
        dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);

        registry.fill(HIST("h_jeth_detatot_part"), detatot, eventWeight);
        registry.fill(HIST("h_jeth_deta_part"), deta, eventWeight);
        registry.fill(HIST("h_jeth_dphi_part"), dphi, eventWeight);
        registry.fill(HIST("h2_jeth_detatot_dphi_part"), detatot, dphi, eventWeight);
        registry.fill(HIST("h2_jeth_deta_dphi_part"), deta, dphi, eventWeight);
        registry.fill(HIST("thn_jethadron_correlations_part"), ptLeadingCorr, ptSubleadingCorr, particle.pt(), detatot, deltaEtaJets, deta, dphi, eventWeight);
        if (particle.pt() < particleJethCut) {
          if (std::abs(deltaEtaJets) >= etaGapup)
            registry.fill(HIST("h2_jeth_physicalcutsup_deta_dphi_part"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) >= etaGapdw && std::abs(deltaEtaJets) < etaGapup)
            registry.fill(HIST("h2_jeth_physicalcutsmd_deta_dphi_part"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) < etaGapdw)
            registry.fill(HIST("h2_jeth_physicalcutsdw_deta_dphi_part"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) >= etaGapdw && etaJet1Raw > etaJet2Raw && etaJet2Raw >= 0)
            registry.fill(HIST("h2_jeth_physicalcutsHup_deta_dphi_part"), deta, dphi, eventWeight);
          if (std::abs(deltaEtaJets) < etaGapdw && etaJet1Raw > etaJet2Raw && etaJet2Raw >= 0)
            registry.fill(HIST("h2_jeth_physicalcutsHdw_deta_dphi_part"), deta, dphi, eventWeight);
        }
      }
    }
  }

  template <typename TmcCollisions, typename TJets, typename TParticles>
  void fillMCPMixJetHadronHistograms(const TmcCollisions& mcCollisions, const TJets& jets, const TParticles& particles, float eventWeight = 1.0)
  {
    auto particlesTuple = std::make_tuple(jets, particles);
    Pair<TmcCollisions, TJets, TParticles, BinningTypeMC> pairMCData{corrBinningMC, numberEventsMixed, -1, mcCollisions, particlesTuple, &cache};

    int totalmix = 0;
    int totaldijets = 0;
    int totaldijetscut = 0;
    int totalPairs = 0;
    int passedPairs = 0;

    for (const auto& [c1, jets1, c2, particles2] : pairMCData) {
      totalmix++;
      registry.fill(HIST("h_mixmc_event_stats"), 1);
      int poolBin = corrBinning.getBin(std::make_tuple(c2.posZ(), c2.multFT0A()));

      //      std::optional<typename TJets::iterator> leadingJet;
      //      std::optional<typename TJets::iterator> subleadingJet;
      using JetType = std::decay_t<decltype(*jets.begin())>;
      std::optional<JetType> leadingJet;
      std::optional<JetType> subleadingJet;
      double ptLeadingCorr = -1.0;
      double ptSubleadingCorr = -1.0;

      //====Step1: find leading/subleading jets==========
      for (auto const& jet : jets1) {
        if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax))
          continue;
        using ParticlesTable = std::decay_t<decltype(particles)>;
        if (!isAcceptedJet<ParticlesTable>(jet, true))
          continue;

        double ptCorr = jet.pt() - jet.area() * c1.rho();
        if (ptCorr > ptLeadingCorr) {
          subleadingJet = leadingJet;
          ptSubleadingCorr = ptLeadingCorr;
          leadingJet = jet;
          ptLeadingCorr = ptCorr;
        } else if (ptCorr > ptSubleadingCorr) {
          subleadingJet = jet;
          ptSubleadingCorr = ptCorr;
        }
      }
      if (!leadingJet || !subleadingJet)
        return;

      double deltaPhiJets = leadingJet->phi() - subleadingJet->phi();
      if (std::abs(deltaPhiJets) < o2::constants::math::PIHalf)
        return;
      registry.fill(HIST("h_mixmc_dijet_dphi"), deltaPhiJets, eventWeight);
      deltaPhiJets = RecoDecay::constrainAngle(deltaPhiJets, 0.);

      totaldijets++;
      registry.fill(HIST("h_mixmc_event_stats"), 2);

      //====Step2: flip in eta==========
      double etaJet1Raw = leadingJet->eta();
      double etaJet2Raw = subleadingJet->eta();
      double flip = (etaJet1Raw > etaJet2Raw) ? 1.0 : -1.0;
      double etajet1 = flip * etaJet1Raw;
      double etajet2 = flip * etaJet2Raw;
      double deltaEtaJetsNoflip = etaJet1Raw - etaJet2Raw;
      double deltaEtaJets = etajet1 - etajet2;

      registry.fill(HIST("h_mixmc_leadjet_corrpt"), ptLeadingCorr, eventWeight);
      registry.fill(HIST("h_mixmc_subleadjet_corrpt"), ptSubleadingCorr, eventWeight);

      if (ptLeadingCorr > leadingjetptMin && ptSubleadingCorr > subleadingjetptMin) {
        totaldijetscut++;
        registry.fill(HIST("h_mixmc_event_stats"), 3);
        registry.fill(HIST("h_mixmc_dijet_pair_counts_cut"), 1);

        registry.fill(HIST("h_mixmc_leadjet_eta"), etaJet1Raw, eventWeight);
        registry.fill(HIST("h_mixmc_subleadjet_eta"), etaJet2Raw, eventWeight);
        registry.fill(HIST("h2_mixmc_dijet_detanoflip_dphi"), deltaEtaJetsNoflip, deltaPhiJets, eventWeight);
        registry.fill(HIST("h2_mixmc_dijet_deta_dphi"), deltaEtaJets, deltaPhiJets, eventWeight);
        registry.fill(HIST("h2_mixmc_dijet_Asymmetry"), ptSubleadingCorr, ptSubleadingCorr / ptLeadingCorr, eventWeight);

        for (auto const& particle : particles2) {
          totalPairs++;
          registry.fill(HIST("h_mixmc_event_stats"), 4);

          double detatot = particle.eta() - etaJet1Raw;
          double deta = flip * (particle.eta() - etaJet1Raw);
          double dphi = particle.phi() - leadingJet->phi();
          dphi = RecoDecay::constrainAngle(dphi, -o2::constants::math::PIHalf);

          passedPairs++;
          registry.fill(HIST("h_mixmc_event_stats"), 5);

          registry.fill(HIST("h_mixmc_jeth_detatot"), detatot, eventWeight);
          registry.fill(HIST("h_mixmc_jeth_deta"), deta, eventWeight);
          registry.fill(HIST("h_mixmc_jeth_dphi"), dphi, eventWeight);
          registry.fill(HIST("h2_mixmc_jeth_detatot_dphi"), detatot, dphi, eventWeight);
          registry.fill(HIST("h2_mixmc_jeth_deta_dphi"), deta, dphi, eventWeight);
          registry.fill(HIST("thn_mixmc_jethadron"), ptLeadingCorr, ptSubleadingCorr, particle.pt(), detatot, deltaEtaJets, deta, dphi, poolBin, eventWeight);
        }
      }
    }

    registry.fill(HIST("h_mixmc_event_stats"), 6, totalmix);
    registry.fill(HIST("h_mixmc_event_stats"), 7, totaldijets);
    registry.fill(HIST("h_mixmc_event_stats"), 8, totaldijetscut);
    registry.fill(HIST("h_mixmc_event_stats"), 9, totalPairs);
    registry.fill(HIST("h_mixmc_event_stats"), 10, passedPairs);
  }

  void processQC(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                 FilterJetTracks const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& track : tracks) {
      if (!jetderiveddatautilities::selectTrack(track, trackSelection)) {
        continue;
      }
      fillTrackHistograms(track);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processQC, "collisions and track QC for Data and MCD", false);

  void processQCWeighted(soa::Join<aod::JetCollisions, aod::JMcCollisionLbs>::iterator const& collision,
                         aod::JetMcCollisions const&,
                         FilterJetTracks const& tracks)
  {
    float eventWeight = collision.weight();
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    //  if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
    //    return;
    //  }  //20250814
    if (std::abs(collision.posZ()) > vertexZCut) {
      return;
    }
    for (auto const& track : tracks) {
      if (!jetderiveddatautilities::selectTrack(track, trackSelection)) {
        continue;
      }
      fillTrackHistograms(track, eventWeight);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processQCWeighted, "weighted collsions and tracks QC for MC", false);

  void processCollisions(soa::Filtered<aod::JetCollisions>::iterator const& collision)
  {
    registry.fill(HIST("h_collisions"), 0.5);
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    registry.fill(HIST("h_collisions"), 1.5);
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    registry.fill(HIST("h_collisions"), 2.5);
    registry.fill(HIST("h2_centrality_occupancy"), collision.centFT0M(), collision.trackOccupancyInTimeRange());
    registry.fill(HIST("h_collisions_Zvertex"), collision.posZ());
    registry.fill(HIST("h_collisions_multFT0M"), collision.multFT0M()); // collision.MultFT0M()
  }
  PROCESS_SWITCH(ChargedJetHadron, processCollisions, "collisions Data and MCD", true);

  void processCollisionsWeighted(soa::Join<aod::JetCollisions, aod::JMcCollisionLbs>::iterator const& collision,
                                 aod::JetMcCollisions const&)
  {
    if (!collision.has_mcCollision()) {
      registry.fill(HIST("h_fakecollisions"), 0.5);
    }
    float eventWeight = collision.weight();
    registry.fill(HIST("h_collisions"), 0.5);
    registry.fill(HIST("h_collisions_weighted"), 0.5, eventWeight);
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    registry.fill(HIST("h_collisions"), 1.5);
    registry.fill(HIST("h_collisions_weighted"), 1.5, eventWeight);
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    registry.fill(HIST("h_collisions"), 2.5);
    registry.fill(HIST("h_collisions_weighted"), 2.5, eventWeight);
    registry.fill(HIST("h2_centrality_occupancy"), collision.centFT0M(), collision.trackOccupancyInTimeRange());
    registry.fill(HIST("h_collisions_Zvertex"), collision.posZ(), eventWeight);
  }
  PROCESS_SWITCH(ChargedJetHadron, processCollisionsWeighted, "weighted collsions for MCD", false);

  void processSpectraData(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                          CorrChargedJets const& jets,
                          aod::JetTracks const&)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillJetHistograms(jet);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraData, "jet spectra for Data", false);

  void processSpectraMCD(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                         CorrChargedMCDJets const& jets,
                         aod::JetTracks const&)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillJetHistograms(jet);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraMCD, "jet spectra for MCD", false);

  void processSpectraAreaSubData(FilterCollision const& collision,
                                 CorrChargedJets const& jets,
                                 aod::JetTracks const&)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillJetAreaSubHistograms(jet, collision.rho());
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraAreaSubData, "jet spectra with rho-area subtraction for Data", false);

  void processJetHadronCorrelation(FilterCollision const& collision,
                                   CorrChargedJets const& jets,
                                   FilterJetTracks const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    fillJetHadronHistograms(collision, jets, tracks);
  }
  PROCESS_SWITCH(ChargedJetHadron, processJetHadronCorrelation, "jet-h for Data", false);

  void processMixJetHadronCorrelation(FilterCollisions const& collisions,
                                      CorrChargedJets const& jets,
                                      FilterJetTracks const& tracks)
  {
    for (auto const& collision : collisions) {
      if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
        continue;
      }
      if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
        continue;
      }
    }
    fillMixJetHadronHistograms(collisions, jets, tracks);
  }
  PROCESS_SWITCH(ChargedJetHadron, processMixJetHadronCorrelation, "jet-h mixed event correlation for Data", false);

  void processLeadingJetHadronCorrelation(FilterCollision const& collision,
                                   CorrChargedJets const& jets,
                                   FilterJetTracks const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    fillLeadingJetHadronHistograms(collision, jets, tracks);
  }
  PROCESS_SWITCH(ChargedJetHadron, processLeadingJetHadronCorrelation, "leadingjet-h for Data", false);

  //////////////below is mixed events 20250919//////////////////////////
  void processMixLeadingJetHadronCorrelation(FilterCollisions const& collisions,
                                      CorrChargedJets const& jets,
                                      FilterJetTracks const& tracks)
  {
    for (auto const& collision : collisions) {
      if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
        continue;
      }
      if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
        continue;
      }
      registry.fill(HIST("h_collisions_mult"), collision.multNTracksGlobal());
    }
    fillMixLeadingJetHadronHistograms(collisions, jets, tracks);
  }
  PROCESS_SWITCH(ChargedJetHadron, processMixLeadingJetHadronCorrelation, "leadingjet-h mixed event correlation for Data", false);

  //////////////up is mixed events //////////////////////////

  void processHFJetCorrelation(FilterCollision const& collision,
                               CorrChargedJets const& jets,
                               aod::CandidatesD0Data const& candidates)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (const auto& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      registry.fill(HIST("h_d0jet_pt"), jet.pt());
      registry.fill(HIST("h_d0jet_corrpt"), jet.pt() - collision.rho() * jet.area());
      registry.fill(HIST("h_d0jet_eta"), jet.eta());
      registry.fill(HIST("h_d0jet_phi"), jet.phi());
    }
    for (const auto& candidate : candidates) {
      // float ptD = candidate.ptD(); //from HFC D0-h
      // float bdtScorePromptD0 = candidate.mlScorePromptD0();
      // float bdtScoreBkgD0 = candidate.mlScoreBkgD0();
      registry.fill(HIST("h_d0_mass"), candidate.m());
      registry.fill(HIST("h_d0_pt"), candidate.pt());
      registry.fill(HIST("h_d0_eta"), candidate.eta());
      registry.fill(HIST("h_d0_phi"), candidate.phi());
      for (const auto& jet : jets) {
        double deltaeta = candidate.eta() - jet.eta();
        double deltaphi = candidate.phi() - jet.phi();
        deltaphi = RecoDecay::constrainAngle(deltaphi, -o2::constants::math::PIHalf);
        registry.fill(HIST("h2_d0jet_detadphi"), deltaeta, deltaphi);
      }
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processHFJetCorrelation, "D0-jet for Data", false);

  void processSpectraAreaSubMCD(FilterCollision const& collision,
                                FilterCollisions const& collisions,
                                CorrChargedMCDJets const& jets,
                                FilterJetTracks const& tracks)
  // aod::JetTracks const& tracks)
  // soa::Filtered<aod::JetTracks> const& tracks)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      fillJetAreaSubHistograms(jet, collision.rho());
    }
    fillLeadingJetHadronHistograms(collision, jets, tracks);
    fillMixLeadingJetHadronHistograms(collisions, jets, tracks);
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraAreaSubMCD, "jet spectra with rho-area subtraction for MCD", false);

  void processSpectraMCDWeighted(soa::Filtered<aod::JetCollisions>::iterator const& collision,
                                 soa::Join<aod::ChargedMCDetectorLevelJets, aod::ChargedMCDetectorLevelJetConstituents, aod::ChargedMCDetectorLevelJetEventWeights> const& jets,
                                 aod::JetTracks const&)
  {
    if (!jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
      return;
    }
    if (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMin || trackOccupancyInTimeRangeMax < collision.trackOccupancyInTimeRange()) {
      return;
    }
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetTracks>(jet)) {
        continue;
      }
      float jetweight = jet.eventWeight();
      float pTHat = 10. / (std::pow(jetweight, 1.0 / pTHatExponent));
      if (jet.pt() > pTHatMaxMCD * pTHat) {
        return;
      }
      registry.fill(HIST("h_jet_phat"), pTHat);
      registry.fill(HIST("h_jet_phat_weighted"), pTHat, jetweight);
      fillJetHistograms(jet, jetweight);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraMCDWeighted, "jet finder QA mcd with weighted events", false);

  void processSpectraMCP(aod::JetMcCollision const& mccollision,
                         soa::SmallGroups<aod::JetCollisionsMCD> const& collisions,
                         CorrChargedMCPJets const& jets,
                         aod::JetParticles const&)
  {
    bool mcLevelIsParticleLevel = true;

    registry.fill(HIST("h_mcColl_counts"), 0.5);
    if (std::abs(mccollision.posZ()) > vertexZCut) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 1.5);
    if (collisions.size() < 1) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 2.5);

    bool hasSel8Coll = false;
    bool centralityIsGood = false;
    bool occupancyIsGood = false;
    for (auto const& collision : collisions) {
      if (jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
        hasSel8Coll = true;
      }
      if ((centralityMin < collision.centFT0M()) && (collision.centFT0M() < centralityMax)) {
        centralityIsGood = true;
      }
      if ((trackOccupancyInTimeRangeMin < collision.trackOccupancyInTimeRange()) && (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMax)) {
        occupancyIsGood = true;
      }
    }
    if (!hasSel8Coll) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 3.5);
    if (!centralityIsGood) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 4.5);
    if (!occupancyIsGood) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 5.5);
    registry.fill(HIST("h_mc_zvertex"), mccollision.posZ());
    registry.fill(HIST("h_mc_mult"), mccollision.multFT0A());

    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetParticles>(jet, mcLevelIsParticleLevel)) {
        continue;
      }
      fillMCPHistograms(jet);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraMCP, "jet spectra for MC particle level", false);

  void processSpectraAreaSubMCP(McParticleCollision::iterator const& mccollision,
                                // McParticleCollision const& mccollisions,
                                soa::SmallGroups<aod::JetCollisionsMCD> const& collisions,
                                CorrChargedMCPJets const& jets,
                                aod::JetParticles const& particles) // need to check 20250915
  // aod::JetParticles const&)
  // soa::Filtered<aod::JetParticles> const& mcparticles)
  {
    bool mcLevelIsParticleLevel = true;

    registry.fill(HIST("h_mcColl_counts_areasub"), 0.5);
    if (std::abs(mccollision.posZ()) > vertexZCut) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 1.5);
    if (collisions.size() < 1) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 2.5);
    if (acceptSplitCollisions == 0 && collisions.size() > 1) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 3.5);

    bool hasSel8Coll = false;
    bool centralityIsGood = false;
    bool occupancyIsGood = false;
    float centrality = -999.0;
    int recoCollision = 2;
    if (acceptSplitCollisions == recoCollision) {
      centrality = collisions.begin().centFT0M();
      if (jetderiveddatautilities::selectCollision(collisions.begin(), eventSelectionBits, skipMBGapEvents)) {
        hasSel8Coll = true;
      }
      if ((centralityMin < collisions.begin().centFT0M()) && (collisions.begin().centFT0M() < centralityMax)) {
        centralityIsGood = true;
      }
      if ((trackOccupancyInTimeRangeMin < collisions.begin().trackOccupancyInTimeRange()) && (collisions.begin().trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMax)) {
        occupancyIsGood = true;
      }
    } else {
      for (auto const& collision : collisions) {
        centrality = collision.centFT0M();
        if (jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
          hasSel8Coll = true;
        }
        if ((centralityMin < collision.centFT0M()) && (collision.centFT0M() < centralityMax)) {
          centralityIsGood = true;
        }
        if ((trackOccupancyInTimeRangeMin < collision.trackOccupancyInTimeRange()) && (collision.trackOccupancyInTimeRange() < trackOccupancyInTimeRangeMax)) {
          occupancyIsGood = true;
        }
      }
    }
    if (!hasSel8Coll) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 4.5);

    if (!centralityIsGood) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 5.5);

    if (!occupancyIsGood) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts_areasub"), 6.5);
    registry.fill(HIST("h_mcColl_rho"), mccollision.rho());
    registry.fill(HIST("h_mcColl_centrality"), centrality);
    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetParticles>(jet, mcLevelIsParticleLevel)) {
        continue;
      }
      fillMCPAreaSubHistograms(jet, mccollision.rho());
    }
    fillMCPLeadingJetHadronHistograms(mccollision, jets, particles);
    //    fillMCPMixLeadingJetHadronHistograms(collisions, jets, particles);
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraAreaSubMCP, "jet spectra with area-based subtraction for MC particle level", false);

  void processSpectraMCPWeighted(aod::JetMcCollision const& mccollision,
                                 soa::SmallGroups<aod::JetCollisionsMCD> const& collisions,
                                 soa::Join<aod::ChargedMCParticleLevelJets, aod::ChargedMCParticleLevelJetConstituents, aod::ChargedMCParticleLevelJetEventWeights> const& jets,
                                 aod::JetParticles const&)
  {
    bool mcLevelIsParticleLevel = true;
    const int ptHadbins = 21;
    float eventWeight = mccollision.weight();

    registry.fill(HIST("h_mcColl_counts"), 0.5);
    registry.fill(HIST("h_mcColl_counts_weight"), 0.5, eventWeight);
    if (std::abs(mccollision.posZ()) > vertexZCut) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 1.5);
    registry.fill(HIST("h_mcColl_counts_weight"), 1.5, eventWeight);
    if (collisions.size() < 1) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 2.5);
    registry.fill(HIST("h_mcColl_counts_weight"), 2.5, eventWeight);

    bool hasSel8Coll = false;
    for (auto const& collision : collisions) {
      if (jetderiveddatautilities::selectCollision(collision, eventSelectionBits, skipMBGapEvents)) {
        hasSel8Coll = true;
      }
    }
    if (!hasSel8Coll) {
      return;
    }
    registry.fill(HIST("h_mcColl_counts"), 3.5);
    registry.fill(HIST("h_mcColl_counts_weight"), 3.5, eventWeight);

    for (auto const& jet : jets) {
      if (!jetfindingutilities::isInEtaAcceptance(jet, jetEtaMin, jetEtaMax, trackEtaMin, trackEtaMax)) {
        continue;
      }
      if (!isAcceptedJet<aod::JetParticles>(jet, mcLevelIsParticleLevel)) {
        continue;
      }
      float jetweight = jet.eventWeight();
      double pTHat = 10. / (std::pow(jetweight, 1.0 / pTHatExponent));
      for (int N = 1; N < ptHadbins; N++) {
        if (jet.pt() < N * 0.25 * pTHat && jet.r() == round(selectedJetsRadius * 100.0f)) {
          registry.fill(HIST("h2_jet_ptcut_part"), jet.pt(), N * 0.25, jetweight);
        }
      }
      registry.fill(HIST("h_jet_phat_part_weighted"), pTHat, jetweight);
      fillMCPHistograms(jet, jetweight);
    }
  }
  PROCESS_SWITCH(ChargedJetHadron, processSpectraMCPWeighted, "jet spectra for MC particle level weighted", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<ChargedJetHadron>(cfgc)};
}
