
library("lattice", lib.loc="~/R/win-library/3.4")
library(readr)
library("corrgram", lib.loc="~/R/win-library/3.4")
library("scatterplot3d", lib.loc="~/R/win-library/3.4")

out <- as.data.frame(read_csv("C:\\Users\\Bryan\\Documents\\starcraft\\bwapi-data\\read\\output.txt", col_names = FALSE))
#out_2 <- as.data.frame(read_csv("C:\\Users\\Bryan\\Documents\\starcraft\\bwapi-data\\write\\output.txt", col_names = FALSE))
#out<-rbind(out,out_2)
dim(out)
names(out)<- c("delta_gas","gamma_supply","alpha_army","alpha_econ","alpha_tech","r","Race","Winner","shortct","medct","lct","opponent_name","map", "build_order")
 # names(out_2)<- c("delta_gas","gamma_supply","alpha_army","alpha_econ","alpha_tech","Race","Winner","shortct","medct","lct","opponent_name","map", "build_order")
# 
 # out<-rbind(out, out_2)
 # out<-out[!duplicated(out),]
# out<-out[which( !out$opponent_name %in% c("Jormungand Brood" ,  "Auriga Tribe"  ,     "Epsilon Squadron" ,  "Sargas Tribe"    ,   "Furinax Tribe"
#                                          , "Ara Tribe"  ,        "Akilae Tribe"   ,    "Garm Brood"    ,     "Antiga"      ,       "Surtur Brood"
#                                          , "Leviathan Brood"  ,  "Elite Guard"   ,     "Baelrog Brood"    ,  "Tiamat Brood"  ,     "Velari Tribe"
#                                          , "Mar Sara"      ,    "Shelak Tribe"   ,    "Cronus Wing"    ,    "Fenris Brood"   ,    "Kel-Morian Combine"
# , "Grendel Brood"   ,   "Venatir Tribe" ,     "Atlas Wing"   ,      "Delta Squadron"  )),]
 # write.table(out,"C:/Users/Bryan/Documents/starcraft/bwapi-data/write/output.txt", sep= ",", quote = FALSE, row.names = FALSE, col.names = FALSE)
# # # out<-new
#  
#(out$shortct>=321 | out$Winner==1) -> out$Winner
out$race_win <- factor( paste ( out$Winner, out$Race , sep= " " ))
out$race_opp <- factor( paste ( out$Race, abbreviate(out$opponent_name) , sep= " " ))
out$race_map <- factor( paste ( out$map, out$Race , sep= " " ))
out$race_build <- factor( paste ( abbreviate(out$build_order), out$Race , sep= " " ))
out$build_map <- factor( paste ( abbreviate(out$build_order), out$map , sep= " " ))
out$build_map_race <- factor( paste ( abbreviate(out$build_order), out$map, out$Race , sep= " " ))
out$opp_map <- factor( paste ( out$map, out$opponent_name , sep= " " ))

#out<-subset(out,out$Winner==1)

# histogram( ~out$delta_gas | out$race_win, type="count") #
# histogram( ~out$gamma_supply | out$race_win, type="count") #
# histogram( ~out$alpha_vis | out$race_win, type="count") #0.6
# histogram( ~out$alpha_army | out$race_win, type="count") #0.525
# histogram( ~out$alpha_econ | out$race_win, type="count") #0.008-0.009
# histogram( ~out$alpha_tech | out$race_win, type="count") #0.002 almost exactly.
# histogram( ~out$r | out$race_win, type="count") #0.00014 almost exactly.

# 
# by(out, out$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army * alpha_econ * alpha_tech - alpha_army - alpha_econ - alpha_tech - alpha_army:alpha_econ:alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# #by(out, out$Race, function(x) summary(glm( Winner ~ delta_gas + gamma_supply + alpha_army + alpha_econ + alpha_tech, family = quasibinomial(link = "logit"), data=x)))
# 
# aggregate( out[, c(1:6,8:12)], list(out$race_win), FUN=mean)
# 
# ks.test( c(out$delta_gas[out$Winner==1]), c(out$delta_gas[out$Winner==0]) )
# ks.test( c(out$gamma_supply[out$Winner==1]), c(out$gamma_supply[out$Winner==0]) )
# ks.test( c(out$alpha_army[out$Winner==1]), c(out$alpha_army[out$Winner==0]) )
 ks.test( c(out$alpha_econ[out$Winner==1]), c(out$alpha_econ[out$Winner==0]) )
 ks.test( c(out$alpha_tech[out$Winner==1]), c(out$alpha_tech[out$Winner==0]) )
# 
 # histogram( ~ out$Winner | out$`Race`, xlab="Win % of Bot vs each Race")
 # histogram( ~ out$Winner | out$`opponent_name`, xlab="Win % of Bot vs each NAME")
 # histogram( ~ out$Winner | out$`race_opp`, xlab="Win % of Bot vs each Race")
 # histogram( ~ out$Winner | abbreviate(out$`build_order`), xlab="Win % of Bot vs each Opening")
 # histogram( ~ out$Winner | out$`map`, xlab="Win % of Bot vs each Map")
 # histogram( ~ out$Winner | abbreviate(out$`race_map`), xlab="Win % of Bot vs each Map")
 
 table(abbreviate(out$build_order), out$Winner)
 table(out$race_build, out$Winner)
 table(out$race_map, out$Winner)
 table(out$build_map, out$Winner)
 table(out$opponent_name,out$Winner)
 table(out$build_map_race,out$Winner)
 table(out$opp_map,out$Winner)
 
# # summary(out)
 out$TotalWins<-cumsum(out$Winner)
 #subset so the parental generation doesn't count.
 parents<-out[out$TotalWins <= 50,]
 out<-out[(nrow(parents)+1):(5000+nrow(parents)),]
 out<-out[complete.cases(out),]
 # colors <- c("#999999", "#E69F00", "#56B4E9") 
 # unique(as.factor(out$Race[out$Winner==1])) #grey=p orange=t blue=z
 # colors <- colors[as.factor(out$Race[out$Winner==1])]
 # for(i in seq(0,360,60)){
 #   scatterplot3d(out$alpha_army[out$Winner==1],
 #                 out$alpha_tech[out$Winner==1],
 #                 out$alpha_econ[out$Winner==1],
 #                 xlab = "Alpha Army",
 #                 ylab = "Alpha Tech",
 #                 zlab = "Alpha Econ",
 #                 angle = i,
 #                 scale.y = 1,
 #                 pch = 16, type="h",
 #                 color = colors,
 #                 #axis.scales = TRUE,
 #                 xlim = c(0,1), ylim = c(0,1), zlim = c(0,1)
 #   )
 # } #[out$Winner==1] , [out$Winner==0], or none
 
 par(mar=c(1,1,1,1))
 
 mav <- function(x,n=5){filter(x,rep(1/n,n), sides=2)}
 plot(mav(out$Winner[out$Race == "Zerg"] , 5))
 plot(mav(out$Winner[out$Race == "Terran"] , 5))
 plot(mav(out$Winner[out$Race == "Protoss"] , 0.1*nrow(out)), ylab="Win Rate (500-game Moving Average)")
 PP.test(out$Winner[out$Race == "Protoss"]) # test if process has a unit root. Robust to arbitrary heteroskedacity.
 
 t_z= 1:nrow(out[out$Race == "Zerg",]); t2 = t_z^2; t3 = t_z^3;
 reg_z<-glm(out$Winner[out$Race == "Zerg"] ~ t_z , family = binomial(link = "logit") )
# summary(reg_z)
t_t= 1:nrow(out[out$Race == "Terran",]); t2 = t_t^2; t3 = t_t^3;
reg_t<-glm(out$Winner[out$Race == "Terran"] ~ t_t   , family = binomial(link = "logit") )

 t_p= 1:nrow(out[out$Race == "Protoss",]); t2 = t_p^2;  t3 = t_p^3;
 reg_p<-glm(out$Winner[out$Race == "Protoss"] ~ t_p , family = binomial(link = "logit") )

plot(t_t,out$Winner[out$Race == "Terran"] ); #protoss should be longest, hopefully.
 points(t_z, reg_z$fitted.values, col = "#999999", type="l" )
 points(t_z, out$Winner[out$Race == "Zerg"], col = "#999999") #grey

 points(t_t, reg_t$fitted.values, col = "#E69F00", type="l" )
 points(t_t, out$Winner[out$Race == "Terran"], col =  "#E69F00") #orange
 
 points(t_p, reg_p$fitted.values, col = "#56B4E9", type="l" )
 points(t_p, out$Winner[out$Race == "Protoss"], col =  "#56B4E9") #blue
 summary(reg_p)


 corrgram(out[(0.95*nrow(out)):nrow(out),][out$Winner==1 & out$build_map_race == out$build_map_race[1],c(1:5)], lower.panel = panel.pts, upper.panel = panel.conf, diag.panel = panel.density)
 #corrgram(out[out$Winner==1 ,c(1:7, 12:13)], lower.panel = panel.pts, upper.panel = panel.conf, diag.panel = panel.density)
par(mar=c(1,1,1,1))

summary(parents)

summary(parents[parents$Winner == 1, ])
summary(out[1:(0.05*nrow(out)),])
summary(out[(0.95*nrow(out)):nrow(out),])

# plot(out$alpha_tech[out$Winner==1], out$alpha_econ[out$Winner==1])
# econ<-c(1,1000)
# army<-c(1,1000)
# tech<-c(1,1000)
# for(i in 1:1000){
#   a<-runif(1)*0.50+0.50
#   b<-runif(1)*0.5
#   c<-runif(1)*0.75+0.25
#   econ[i]<-a/(a+b+c)
#   tech[i]<-b/(a+b+c)
#   army[i]<-c/(a+b+c)}
# hist(econ)
# hist(army)
# hist(tech)
dim(out)

