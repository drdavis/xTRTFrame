// ATLAS
#include <EventLoop/Job.h>
#include <EventLoop/StatusCode.h>
#include <AsgTools/MessageCheck.h>
#include <xAODRootAccess/Init.h>
#include <xAODRootAccess/TEvent.h>

// ROOT
#include <TSystem.h>
#include <TFile.h>

// TRTFrame
#include <TRTFrame/TruthLoop.h>

ClassImp(TRTF::TruthLoop)

TRTF::TruthLoop::TruthLoop() : TRTF::LoopAlg() {
  m_fillLeptonsOnly = false;
  m_saveHits        = false;
}

TRTF::TruthLoop::~TruthLoop() {}

EL::StatusCode TRTF::TruthLoop::histInitialize() {
  ANA_CHECK_SET_TYPE(EL::StatusCode);
  ANA_CHECK(TRTF::LoopAlg::histInitialize());

  book(TH1F("h_averageMu","",70,-0.5,69.5));

  TFile *outFile = wk()->getOutputFile(m_outputName);

  m_elTree = new TTree("elTree","elTree");
  m_muTree = new TTree("muTree","muTree");
  m_piTree = new TTree("piTree","piTree");
  m_otTree = new TTree("otTree","otTree");
  m_zdTree = new TTree("zdTree","zdTree");
  m_jdTree = new TTree("jdTree","jdTree");

  for ( TTree* itree : { m_elTree, m_muTree, m_piTree, m_otTree, m_zdTree, m_jdTree } ) {
    itree->SetDirectory(outFile);
    itree->Branch("weight",     &m_weight);
    itree->Branch("pdgId",      &m_pdgId);
    itree->Branch("trkOcc",     &m_trkOcc);
    itree->Branch("truthMass",  &m_truthMass);
    itree->Branch("p",          &m_p);
    itree->Branch("pT",         &m_pT);
    itree->Branch("eta",        &m_eta);
    itree->Branch("phi",        &m_phi);
    itree->Branch("theta",      &m_theta);
    itree->Branch("eProbHT",    &m_eProbHT);
    itree->Branch("eProbToT",   &m_eProbToT);
    itree->Branch("eProbToT2",  &m_eProbToT2);
    itree->Branch("eProbComb",  &m_eProbComb);
    itree->Branch("eProbComb2", &m_eProbComb2);
    itree->Branch("nTRThits",   &m_nTRThits);
    itree->Branch("nArhits",    &m_nArhits);
    itree->Branch("nXehits",    &m_nXehits);
    itree->Branch("nHThits",    &m_nHThits);
    itree->Branch("dEdxNoHT",   &m_dEdxNoHT);
    itree->Branch("NhitsdEdx",  &m_nHitsdEdx);
    itree->Branch("sumToT",     &m_sumToT);
    itree->Branch("sumL",       &m_sumL);

    if ( m_saveHits ) {
      itree->Branch("hit_HTMB",       &m_HTMB);
      itree->Branch("hit_gasType",    &m_gasType);
      itree->Branch("hit_bec",        &m_bec);
      itree->Branch("hit_layer",      &m_layer);
      itree->Branch("hit_strawlayer", &m_strawlayer);
      itree->Branch("hit_strawnumber",&m_strawnumber);
      itree->Branch("hit_drifttime",  &m_drifttime);
      itree->Branch("hit_tot",        &m_tot);
      itree->Branch("hit_T0",         &m_T0);
      itree->Branch("hit_localTheta", &m_localTheta);
      itree->Branch("hit_localPhi",   &m_localPhi);
      itree->Branch("hit_HitZ",       &m_HitZ);
      itree->Branch("hit_HitR",       &m_HitR);
      itree->Branch("hit_rTrkWire",   &m_rTrkWire);
      itree->Branch("hit_L",          &m_L);
    }
  }

  return EL::StatusCode::SUCCESS;
}

EL::StatusCode TRTF::TruthLoop::initialize() {
  ANA_CHECK_SET_TYPE(EL::StatusCode);
  ANA_CHECK(TRTF::LoopAlg::initialize());
  return EL::StatusCode::SUCCESS;
}

EL::StatusCode TRTF::TruthLoop::execute() {
  ANA_CHECK_SET_TYPE(EL::StatusCode);
  ANA_CHECK(TRTF::LoopAlg::execute());

  bool isMC = false;
  if ( m_eventInfo->eventType(xAOD::EventInfo::IS_SIMULATION) ) {
    isMC = true;
  }
  if ( !isMC && config()->useGRL() ) {
    if ( !m_GRLToolHandle->passRunLB(*m_eventInfo) ) {
      return EL::StatusCode::SUCCESS;
    }
  }

  m_weight = eventWeight();
  hist("h_averageMu")->Fill(averageMu(),m_weight);

  /*
    auto combinedProb = [](float pHT, float pToT) {
    return pHT*pToT/(pHT*pToT+(1-pHT)*(1-pToT));
    };
  */

  auto tracks = trackContainer();

  int nthPion = 0;
  for ( auto const& track : *tracks ) {

    auto truth = truthParticle(track);
    if ( truth == nullptr ) continue;
    auto vtx = track->vertex();
    if ( vtx == nullptr ) continue;
    m_pdgId = std::abs(truth->pdgId());

    if ( m_fillLeptonsOnly && ( m_pdgId != 11 && m_pdgId != 13 ) ) continue;

    bool failTrkSel = false;
    if      ( m_pdgId == 11 ) failTrkSel = !m_trackElecSelToolHandle->accept(*track,vtx);
    else if ( m_pdgId == 13 ) failTrkSel = !m_trackMuonSelToolHandle->accept(*track,vtx);
    else                      failTrkSel = !m_trackSelToolHandle->accept(*track,vtx);
    if ( failTrkSel ) continue;

    uint8_t ntrthits = -1;
    track->summaryValue(ntrthits,xAOD::numberOfTRTHits);
    m_nTRThits = ntrthits;
    m_nHThits  = 0;
    m_sumL     = 0.0;
    m_sumToT   = 0.0;
    //m_nHThits  = nhthits;

    // only take every 5th pion.
    if ( m_pdgId == 211 ) {
      nthPion++;
      if ( nthPion%5 != 0 ) {
        continue;
      }
    }

    m_truthMass = truth->m();
    m_pT        = track->pt();
    m_p         = track->p4().P();
    m_eta       = track->eta();
    m_phi       = track->phi();
    m_theta     = track->theta();

    /*
      auto absEta = std::fabs(m_eta);
      TRTF::StrawType st;
      if      ( absEta < 0.625 ) { st = TRTF::StrawType::BRL; }
      else if ( absEta < 1.070 ) { st = TRTF::StrawType::NON; }
      else if ( absEta < 1.304 ) { st = TRTF::StrawType::ECA; }
      else if ( absEta < 1.752 ) { st = TRTF::StrawType::NON; }
      else if ( absEta < 2.000 ) { st = TRTF::StrawType::ECB; }
      else                       { st = TRTF::StrawType::NON; }
    */

    m_trkOcc    = get(TRTF::Acc::TRTTrackOccupancy,track);
    m_eProbToT  = get(TRTF::Acc::eProbabilityToT,track);
    m_eProbHT   = get(TRTF::Acc::eProbabilityHT,track);
    m_eProbToT2 = 0; //particleIdSvc()->ToT_getTest(get(TRTF::Acc::ToT_dEdx_noHT_divByL,track),
                     //                          m_p,TRTF::Hyp::Electron,TRTF::Hyp::Pion,
                     //                          get(TRTF::Acc::ToT_usedHits_noHT_divByL,track), st);

    m_eProbComb  = 0; //combinedProb(m_eProbHT,m_eProbToT);
    m_eProbComb2 = 0; //combinedProb(m_eProbHT,m_eProbToT2);

    m_dEdxNoHT  = get(TRTF::Acc::ToT_dEdx_noHT_divByL,track);
    m_nHitsdEdx = get(TRTF::Acc::ToT_usedHits_noHT_divByL,track);

    const xAOD::TrackStateValidation* msos = nullptr;
    const xAOD::TrackMeasurementValidation* driftCircle = nullptr;
    if ( TRTF::Acc::msosLink.isAvailable(*track) ) {
      for ( auto trackMeasurement : TRTF::Acc::msosLink(*track) ) {
        if ( trackMeasurement.isValid() ) {
          msos = *trackMeasurement;
          if ( msos->detType() != 3 ) continue; // TRT hits only.
          if ( !(msos->trackMeasurementValidationLink().isValid()) ) continue;
          driftCircle = *(msos->trackMeasurementValidationLink());
          if ( !fillHitBasedVariables(track,msos,driftCircle) ) continue;
        }
      }
    }

    auto parent = truth->parent();
    if ( parent != nullptr ) {
      if ( parent->isZ()          ) { m_zdTree->Fill(); }
      if ( parent->pdgId() == 443 ) { m_jdTree->Fill(); }
    }

    if ( m_fillLeptonsOnly ) {
      if      ( m_pdgId == 11  ) { m_elTree->Fill(); }
      else if ( m_pdgId == 13  ) { m_muTree->Fill(); }
      else                       { continue;         }
    }
    else {
      if      ( m_pdgId == 11  ) { m_elTree->Fill(); }
      else if ( m_pdgId == 13  ) { m_muTree->Fill(); }
      else if ( m_pdgId == 211 ) { m_piTree->Fill(); }
      else                       { m_otTree->Fill(); }
    }

    clearVectors();
  } // loop over tracks

  return EL::StatusCode::SUCCESS;
}

bool TRTF::TruthLoop::fillHitBasedVariables(const xAOD::TrackParticle* track,
                                            const xAOD::TrackStateValidation* msos,
                                            const xAOD::TrackMeasurementValidation* driftCircle) {
  auto hit = getHitSummary(track,msos,driftCircle);
  if ( hit.tot < 0.005 ) return false;
  m_type.push_back(hit.type);
  m_HTMB.push_back(hit.HTMB);
  if ( hit.HTMB == 1 ) m_nHThits++;
  m_gasType.push_back(hit.gasType);
  m_bec.push_back(hit.bec);
  m_layer.push_back(hit.layer);
  m_strawlayer.push_back(hit.strawlayer);
  m_strawnumber.push_back(hit.strawnumber);
  m_drifttime.push_back(hit.drifttime);
  m_tot.push_back(hit.tot);
  m_sumToT += hit.tot;
  m_T0.push_back(hit.T0);

  m_localTheta.push_back(hit.localTheta);
  m_localPhi.push_back(hit.localPhi);
  m_HitZ.push_back(hit.HitZ);
  m_HitR.push_back(hit.HitR);
  m_rTrkWire.push_back(hit.rTrkWire);
  m_L.push_back(hit.L);
  m_sumL += hit.L;
  return true;
}

void TRTF::TruthLoop::clearVectors() {
  m_type       .clear();
  m_HTMB       .clear();
  m_gasType    .clear();
  m_bec        .clear();
  m_layer      .clear();
  m_strawlayer .clear();
  m_strawnumber.clear();
  m_drifttime  .clear();
  m_tot        .clear();
  m_T0         .clear();
  m_localTheta .clear();
  m_localPhi   .clear();
  m_HitZ       .clear();
  m_HitR       .clear();
  m_rTrkWire   .clear();
  m_L          .clear();
}
