import FWCore.ParameterSet.Config as cms

process = cms.Process("TestZMuMuSubskim")

process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.options   = cms.untracked.PSet( wantSummary = cms.untracked.bool(True) )

# source
process.source = cms.Source("PoolSource", 
     fileNames = cms.untracked.vstring(
     'rfio:/castor/cern.ch/user/f/fabozzi/mc7tev/F8EE38AF-1EBE-DE11-8D19-00304891F14E.root'
#    'file:/scratch1/cms/data/summer09/aodsim/zmumu/0016/889E7356-0084-DE11-AF48-001E682F8676.root'
#    'file:testEWKMuSkim.root'
    )
)
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(1000) )

process.load("Configuration.StandardSequences.Geometry_cff")
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
process.GlobalTag.globaltag = cms.string('START3X_V21::All')
process.load("Configuration.StandardSequences.MagneticField_cff")

process.load("ElectroWeakAnalysis.Skimming.zMuMu_SubskimPaths_cff")

# Output module configuration
process.load("ElectroWeakAnalysis.Skimming.zMuMuSubskimOutputModule_cfi")
process.zMuMuSubskimOutputModule.fileName = 'testZMuMuSubskim.root'

# MC matching sequence
process.load("ElectroWeakAnalysis.Skimming.zMuMu_MCTruth_cfi")
process.mcEventContent = cms.PSet(
    outputCommands = cms.untracked.vstring(
    ### MC matching infos
    'keep *_genParticles_*_*',
    'keep *_allDimuonsMCMatch_*_*',
    )
)
process.zMuMuSubskimOutputModule.outputCommands.extend(process.mcEventContent.outputCommands)
############

process.outpath = cms.EndPath(process.zMuMuSubskimOutputModule)


